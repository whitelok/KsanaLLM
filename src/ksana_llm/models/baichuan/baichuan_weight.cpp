/* Copyright 2024 Tencent Inc.  All rights reserved.

==============================================================================*/
#include "ksana_llm/models/baichuan/baichuan_weight.h"

namespace ksana_llm {

template <typename T>
BaichuanWeight<T>::BaichuanWeight(const ModelConfig& model_config, int rank, std::shared_ptr<Context> context) {
  common_weight_ = std::make_shared<CommonWeight<T>>(model_config, rank, context);
}

template <typename T>
Tensor BaichuanWeight<T>::GetModelWeights(const std::string& weight_name) {
  return common_weight_->GetModelWeights(weight_name);
}

template <typename T>
Status BaichuanWeight<T>::LoadWeightsFromFile(std::shared_ptr<BaseFileTensorLoader>& weights_loader,
                                              std::vector<std::string>& weight_name_list,
                                              std::vector<std::string>& custom_name_list) {
  if (!common_weight_->LoadWeightsFromFile(weights_loader, weight_name_list, custom_name_list).OK()) {
    KLLM_THROW(fmt::format("Load weight file {} error.", weights_loader->GetTensorFileName()));
  }
  return Status();
}

template <typename T>
void BaichuanWeight<T>::ProcessWeights() {
  common_weight_->ProcessWeights();
}

template <typename T>
void BaichuanWeight<T>::SetEmbeddingsConfig() {
  common_weight_->SetEmbeddingsConfig();
}

template class BaichuanWeight<float>;
template class BaichuanWeight<float16>;
#ifdef ENABLE_BFLOAT16
template class BaichuanWeight<bfloat16>;
#endif

}  // namespace ksana_llm
