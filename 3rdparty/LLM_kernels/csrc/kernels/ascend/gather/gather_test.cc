/*
 * Copyright 2024 Tencent Inc.  All rights reserved.
 */

#include <gtest/gtest.h>
#include <cmath>

#include "3rdparty/half.hpp"
#include "csrc/kernels/ascend/gather/gather.h"
#include "csrc/utils/ascend/common.h"
#include "tests/kernels/ascend/utils/testsuit_base.h"

using namespace llm_kernels::utils;

namespace llm_kernels {
namespace ascend {
namespace test {

class LlamaAscendGatherTestSuit : public AscendTestSuitBase {
 public:
  void SetUp() override { AscendTestSuitBase::SetUp(); }

  void TearDown() override { AscendTestSuitBase::TearDown(); }

 protected:
  using AscendTestSuitBase::context;
  using AscendTestSuitBase::default_device;
  using AscendTestSuitBase::is_inited;
  using AscendTestSuitBase::stream;
};

TEST_F(LlamaAscendGatherTestSuit, CatTest) {
  const std::vector<int64_t> input_shape = {4, 8};
  aclTensor* input_tensor = nullptr;
  void* input_workspace = nullptr;

  const std::vector<int64_t> output_shape = {1, 8};
  aclTensor* output_tensor = nullptr;
  void* output_workspace = nullptr;

  const std::vector<int64_t> index_shape = {1};
  aclTensor* index_tensor = nullptr;
  void* index_workspace = nullptr;

  CreateAclTensor(input_shape, &input_workspace, aclDataType::ACL_FLOAT16, aclFormat::ACL_FORMAT_ND, &input_tensor);
  CreateAclTensor(index_shape, &index_workspace, aclDataType::ACL_INT64, aclFormat::ACL_FORMAT_ND, &index_tensor);
  CreateAclTensor(output_shape, &output_workspace, aclDataType::ACL_FLOAT16, aclFormat::ACL_FORMAT_ND, &output_tensor);
  std::vector<half_float::half> input_vec_host(GetShapeSize(input_shape));
  std::vector<half_float::half> index_vec_host(GetShapeSize(index_shape));
  std::vector<half_float::half> out_vec_host(GetShapeSize(output_shape));
  for (size_t i = 0; i < input_vec_host.size(); ++i) {
    input_vec_host[i] = (half_float::half)(static_cast<float>(rand()) / static_cast<float>(RAND_MAX));
  }
  index_vec_host[0] = 1;
  ACL_CHECK_RET(aclrtMemcpyAsync(input_workspace, GetShapeSize(input_shape) * sizeof(half_float::half),
                                 input_vec_host.data(), GetShapeSize(input_shape) * sizeof(half_float::half),
                                 ACL_MEMCPY_HOST_TO_DEVICE, stream));
  ACL_CHECK_RET(aclrtMemcpyAsync(index_workspace, GetShapeSize(index_shape) * sizeof(half_float::half),
                                 index_vec_host.data(), GetShapeSize(index_shape) * sizeof(half_float::half),
                                 ACL_MEMCPY_HOST_TO_DEVICE, stream));
  int64_t gather_dim = 0;

  Gather(input_tensor, gather_dim, index_tensor, &output_tensor, stream, llm_kernels::utils::GetTestWorkSpaceFunc);

  ACL_CHECK_RET(aclrtMemcpyAsync(out_vec_host.data(), GetShapeSize(output_shape) * sizeof(half_float::half),
                                 output_workspace, GetShapeSize(output_shape) * sizeof(half_float::half),
                                 ACL_MEMCPY_DEVICE_TO_HOST, stream));
  ACL_CHECK_RET(aclrtSynchronizeStream(stream));

  for (size_t i = 0; i < input_shape[1]; ++i) {
    EXPECT_NEAR(float(input_vec_host[i]), float(out_vec_host[i]), 1e-5);
  }

  ACL_CHECK_RET(aclDestroyTensor(output_tensor));
  ACL_CHECK_RET(aclDestroyTensor(input_tensor));
  ACL_CHECK_RET(aclDestroyTensor(index_tensor));
  ACL_CHECK_RET(aclrtFree(input_workspace));
  ACL_CHECK_RET(aclrtFree(index_workspace));
  ACL_CHECK_RET(aclrtFree(output_workspace));
}

}  // namespace test
}  // namespace ascend
}  // namespace llm_kernels
