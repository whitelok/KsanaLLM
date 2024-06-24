/* Copyright 2024 Tencent Inc.  All rights reserved.

==============================================================================*/
#pragma once

#ifdef ENABLE_FP8
#  include "ksana_llm/layers/base_layer.h"

namespace ksana_llm {

template <typename T>
class Fp8MatMulLayer : public BaseLayer {
 public:
  virtual Status Init(const std::vector<std::any>& parameters, std::shared_ptr<Context> context, int rank) override;

  virtual Status Forward(const std::vector<Tensor>& input_tensors, std::vector<Tensor>& output_tensors) override;

  size_t GetWorkSpaceSize(const int m, const int k);

  virtual size_t GetWorkSpaceSize() override;

 private:
  int max_m_;
  int max_k_;
};

}  // namespace ksana_llm
#endif
