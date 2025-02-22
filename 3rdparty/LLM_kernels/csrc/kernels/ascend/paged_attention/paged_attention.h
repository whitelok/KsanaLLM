/* Copyright 2024 Tencent Inc.  All rights reserved.

==============================================================================*/
#pragma once

#include <stdint.h>
#include <vector>

#include "acl/acl.h"
#include "acl/acl_op_compiler.h"

#include "csrc/kernels/ascend/permute/permute.h"
#include "csrc/kernels/ascend/rotary_embedding/rotary_embedding.h"
#include "csrc/kernels/ascend/slice/slice.h"
#include "csrc/utils/ascend/atb_executor.h"

namespace llm_kernels {
namespace ascend {

// The paged attention implement.
template <typename T>
class PagedAttention {
 public:
  ~PagedAttention();

  // Invoke paged attention.
  void Forward(void* output, void* qkv_tensor, void* seq_offset, void** kv_list, void* block_offset, void* rope_pos,
               int batch_size, int total_token_num, int total_block_num, int layer_index, bool is_multi_token_forward,
               aclrtStream stream);

  // Initialize some necessary information.
  void Initialize(uint32_t head_size, uint32_t kv_head_size, uint32_t head_dim, uint32_t layer_num, uint32_t layer_idx,
                  uint32_t block_token_num, aclrtStream stream,
                  const RotaryEmbeddingType scaling_type = RotaryEmbeddingType::DEFAULT,
                  const float scaling_factor = 1.0f);

 private:
  void GenerateTilingData(bool is_multi_token_forward, uint32_t seq_len, uint32_t seq_block_num, int32_t token_pos);

  // Initialize common tiling data.
  void InitTilingData(bool is_multi_token_forward);

  // Copy the tiling data from host to global memory.
  void CopyTilingToDevice(bool is_multi_token_forward, aclrtStream stream);

  void InitAttnMask();
  void InitPermuteTiling(aclrtStream stream);
  void InitSliceTiling(aclrtStream stream);

 private:
  // The token offset of prefill and decode stage.
  uint64_t* prefill_token_offset_ = nullptr;
  int32_t* decode_tokens_len_ = nullptr;

  // The rope cache, [max_position_embeddings, rotary_dim].
  void* cos_sin_cache_ = nullptr;

  // The kv list pointer.
  void** kv_list_ = nullptr;

  // The offset of kv block.
  int32_t* kv_cache_offset_ = nullptr;

  size_t head_size_;
  size_t kv_head_size_;

  size_t head_dim_;
  size_t layer_num_;
  size_t block_token_num_;

  // ROPE configs.
  size_t max_position_embeddings_ = 2048;
  size_t rotary_dim_ = 128;
  size_t stride_size_;
  size_t rope_base_ = 10000;
  bool is_neox_ = true;

  RotaryEmbeddingType scaling_type_ = RotaryEmbeddingType::DEFAULT;
  float scaling_factor_ = 1.0f;

  // The slice & permute & rope instantiation.
  Slice2<T> slice_;
  PermuteKernelWrapper<T> permute_;
  AscendCRotaryEmbedding<T> rope_;

  // The tiling buffer on global memory
  void* tiling_buffer_gm_;

  void* attn_mask_gm_;

  // Used to cache permute tiling.
  void* permute_tiling_gm_;

  // Used to cache slice tiling.
  void* slice_tiling_gm_;

  // The size of tiling data.
  size_t tiling_size_;

  // The worksapce.
  void* workspace_gm_;

  // The buffer memory
  void* q_buffer_;
  void* k_buffer_;
  void* v_buffer_;
  void* o_buffer_;

  void* q_buffer_2_;
  void* k_buffer_2_;
  void* v_buffer_2_;
};

}  // namespace ascend
}  // namespace llm_kernels
