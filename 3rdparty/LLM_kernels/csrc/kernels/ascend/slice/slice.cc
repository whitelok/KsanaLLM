/*
 * Copyright 2024 Tencent Inc.  All rights reserved.
 */

#include "slice.h"

#include "aclnnop/aclnn_slice.h"
#include "csrc/utils/ascend/common.h"

#include "aclrtlaunch_InvokeSliceKernel.h"

#include "csrc/kernels/ascend/slice/slice_tiling.h"

namespace llm_kernels {
namespace ascend {

// The max block size of slice.
constexpr uint32_t MAX_SLICE_BLOCK_SIZE = 16 * 16;

// The max used ai core number.
constexpr uint32_t MAX_USED_CORE_NUM = 24 * 2;

void Slice(const aclTensor* input, const int sliceDim, const int sliceStart, const int sliceEnd, const int sliceStep,
           aclTensor** output, aclrtStream& stream, void (*ws_func)(size_t, void**)) {
  uint64_t ws_size = 0ull;
  void* workspace = nullptr;
  aclOpExecutor* executor = nullptr;
  ACL_CHECK_RET(
      aclnnSliceGetWorkspaceSize(input, sliceDim, sliceStart, sliceEnd, sliceStep, *output, &ws_size, &executor));
  ws_func(ws_size, &workspace);
  ACL_CHECK_RET(aclnnSlice(workspace, ws_size, executor, stream));
}

template <typename T>
Slice2<T>::Slice2() {
  tiling_size_ = sizeof(SliceTilingData);
  ACL_CHECK_RET(aclrtMalloc(&tiling_buffer_gm_, tiling_size_, ACL_MEM_MALLOC_HUGE_FIRST));

  // TODO: Get block num from device info.
  tiling_data_.used_core_num = 24;
}

template <typename T>
Slice2<T>::~Slice2() {
  ACL_CHECK_RET(aclrtFree(tiling_buffer_gm_));
}

template <typename T>
void Slice2<T>::CopyTilingToDevice(aclrtStream stream) {
  ACL_CHECK_RET(aclrtMemcpyAsync(tiling_buffer_gm_, tiling_size_, &tiling_data_, tiling_size_,
                                 aclrtMemcpyKind::ACL_MEMCPY_HOST_TO_DEVICE, stream));
  ACL_CHECK_RET(aclrtSynchronizeStream(stream));
}

template <typename T>
void Slice2<T>::GenerateTiling(int start, int length, int step, int times, SliceTilingData& tiling_data) {
  tiling_data.start = start * sizeof(T);
  tiling_data.length = length * sizeof(T);
  tiling_data.step = step * sizeof(T);
  tiling_data.times = times;

  if (tiling_data.length < MAX_SLICE_BLOCK_SIZE) {
    tiling_data.block_size = tiling_data.length;
    tiling_data.tail_block_size = tiling_data.length;
    tiling_data.step_block_num = 1;
  } else {
    tiling_data.block_size = MAX_SLICE_BLOCK_SIZE;
    uint32_t tail_size = tiling_data.length % MAX_SLICE_BLOCK_SIZE;
    if (tail_size != 0) {
      tiling_data.tail_block_size = tail_size;
    } else {
      tiling_data.tail_block_size = tiling_data.block_size;
    }
    tiling_data.step_block_num = (tiling_data.length + MAX_SLICE_BLOCK_SIZE - 1) / MAX_SLICE_BLOCK_SIZE;
  }

  if (tiling_data.times * tiling_data.step_block_num < MAX_USED_CORE_NUM) {
    tiling_data.used_core_num = tiling_data.times * tiling_data.step_block_num;
  } else {
    tiling_data.used_core_num = MAX_USED_CORE_NUM;
  }
}

template <typename T>
void Slice2<T>::CacheTiling(void* dev, size_t key, int start, int length, int step, int times, aclrtStream stream) {
  SliceTilingData tiling_data;
  GenerateTiling(start, length, step, times, tiling_data);

  ACL_CHECK_RET(aclrtMemcpyAsync(dev, tiling_size_, &tiling_data, tiling_size_,
                                 aclrtMemcpyKind::ACL_MEMCPY_HOST_TO_DEVICE, stream));
  ACL_CHECK_RET(aclrtSynchronizeStream(stream));

  tiling_cache_[key] = dev;
  tiling_cores_[key] = tiling_data.used_core_num;
}

template <typename T>
void* Slice2<T>::GetTilingData(size_t key, int& block_dim) {
  if (tiling_cache_.find(key) != tiling_cache_.end()) {
    block_dim = tiling_cores_.at(key);
    return tiling_cache_.at(key);
  }
  return nullptr;
}

template <typename T>
void Slice2<T>::Forward(void* output, void* input, void* tiling, int block_dim, aclrtStream stream) {
  ACLRT_LAUNCH_KERNEL(InvokeSliceKernel)(block_dim, stream, input, output, tiling);
}

template <typename T>
void Slice2<T>::Forward(void* output, void* input, int start, int length, int step, int times, aclrtStream stream) {
  GenerateTiling(start, length, step, times, tiling_data_);
  CopyTilingToDevice(stream);

  ACLRT_LAUNCH_KERNEL(InvokeSliceKernel)
  (tiling_data_.used_core_num, stream, input, output, tiling_buffer_gm_);
}

template class Slice2<aclFloat16>;
template class Slice2<float>;

}  // namespace ascend
}  // namespace llm_kernels
