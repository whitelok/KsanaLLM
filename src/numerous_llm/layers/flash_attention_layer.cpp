/* Copyright 2023 Tencent Inc.  All rights reserved.

==============================================================================*/

#include "numerous_llm/layers/flash_attention_layer.h"
#include "numerous_llm/kernels/nvidia/kernel_wrapper.h"

namespace numerous_llm {

Status FlashAttentionLayer::Forward(const std::vector<Tensor>& input_tensors, std::vector<Tensor>& output_tensors) {
  int max_tokens = input_tensors[1].shape[1];
  int batch_size = input_tensors[1].shape[0];
  int total_tokens = input_tensors[0].shape[0];

  size_t qkv_size = input_tensors[0].GetTotalBytes();
  NLLM_LOG_INFO << fmt::format("qkv bytes size = {}", qkv_size);
  void* qkv_ptr = input_tensors[0].GetPtr<void>();

  void* q_ptr = qkv_ptr;
  void* k_ptr = qkv_ptr + qkv_size / 3;
  void* v_ptr = qkv_ptr + qkv_size / 3 * 2;

  AttenVarlen(q_ptr, k_ptr, v_ptr, output_tensors[0].GetPtr<void>(), input_tensors[1].GetPtr<void>(), total_tokens,
              max_tokens, batch_size, num_heads_, head_size_, is_causal_, rank_, context_->GetComputeStreams()[rank_]);
  return Status();
}

}  // namespace numerous_llm
