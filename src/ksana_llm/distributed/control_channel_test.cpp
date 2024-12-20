/* Copyright 2024 Tencent Inc.  All rights reserved.

==============================================================================*/

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cstring>
#include <filesystem>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include "ksana_llm/block_manager/block_manager_interface.h"
#include "ksana_llm/data_hub/data_hub.h"
#include "ksana_llm/data_hub/schedule_output.h"
#include "ksana_llm/distributed/control_channel.h"

#include "ksana_llm/distributed/distributed_test_helper.h"
#include "ksana_llm/utils/environment.h"
#include "ksana_llm/utils/memory_utils.h"
#include "ksana_llm/utils/singleton.h"
#include "ksana_llm/utils/socket_util.h"
#include "ksana_llm/utils/status.h"
#include "test.h"

#include "ksana_llm/helpers/environment_test_helper.h"

using namespace ksana_llm;

class ControlChannelTest : public testing::Test {
 protected:
  void SetUp() override {
    master_env_ = std::make_shared<Environment>();
    worker_env_ = std::make_shared<Environment>();

    // Set model config.
    std::string config_file = GetTestConfigFile();
    Singleton<Environment>::GetInstance()->ParseConfig(config_file);

    master_env_->ParseConfig(config_file);
    worker_env_->ParseConfig(config_file);

    master_schedule_output_pool_ = new ScheduleOutputPool();
    worker_schedule_output_pool_ = new ScheduleOutputPool();

    std::string interface;
    GetAvailableInterfaceAndIP(interface, master_host_);
    GetAvailablePort(master_port_);

    ctrl_channel_master_ = std::make_shared<ControlChannel>(master_host_, master_port_, world_size_, 0, GetPacketObject,
                                                            master_schedule_output_pool_, master_env_);
    ctrl_channel_worker_ = std::make_shared<ControlChannel>(master_host_, master_port_, world_size_, 1, GetPacketObject,
                                                            worker_schedule_output_pool_, worker_env_);
  }

  void TearDown() override {
    ctrl_channel_worker_.reset();
    ctrl_channel_master_.reset();

    delete master_schedule_output_pool_;
    delete worker_schedule_output_pool_;
  }

 protected:
  std::shared_ptr<Environment> master_env_ = nullptr;
  std::shared_ptr<Environment> worker_env_ = nullptr;

  // The schedule output pool.
  ScheduleOutputPool* master_schedule_output_pool_ = nullptr;
  ScheduleOutputPool* worker_schedule_output_pool_ = nullptr;

  std::shared_ptr<ControlChannel> ctrl_channel_master_ = nullptr;
  std::shared_ptr<ControlChannel> ctrl_channel_worker_ = nullptr;

  std::string master_host_;
  uint16_t master_port_;
  size_t world_size_ = 2;
};

TEST_F(ControlChannelTest, TestControlChannel) {
  FakedTestBlockManager* test_block_manager = new FakedTestBlockManager();
  SetBlockManager(test_block_manager);

  size_t master_device_block_num;
  size_t master_host_block_num;
  size_t worker_device_block_num;
  size_t worker_host_block_num;

  uint16_t master_lower_layer_idx;
  uint16_t master_upper_layer_idx;
  uint16_t worker_lower_layer_idx;
  uint16_t worker_upper_layer_idx;

  // master node.
  auto master_fn = [&]() {
    // Start master
    ctrl_channel_master_->Listen();

    // Set block num for master.
    test_block_manager->SetBlockNumber(10, 8);

    // Wait all workers connected.
    ctrl_channel_master_->Barrier();

    // synchronize layers.
    ctrl_channel_master_->SynchronizeNodeLayers();

    // synchronize cache block num.
    ctrl_channel_master_->SynchronizeCacheBlockNum();

    // Close master.
    ctrl_channel_master_->Close();

    PipelineConfig pipeline_config;
    master_env_->GetPipelineConfig(pipeline_config);
    master_device_block_num = pipeline_config.device_block_num;
    master_host_block_num = pipeline_config.host_block_num;

    master_lower_layer_idx = pipeline_config.lower_layer_idx;
    master_upper_layer_idx = pipeline_config.upper_layer_idx;
  };
  std::thread master_thread = std::thread(master_fn);

  // worker node.
  auto worker_fn = [&]() {
    // Start worker
    ctrl_channel_worker_->Connect();

    // Set block num for worker.
    test_block_manager->SetBlockNumber(6, 4);

    // Add worker to cluster.
    ctrl_channel_worker_->AddNode();

    // Wait all workers connected.
    ctrl_channel_worker_->Barrier();

    // Wait layer result.
    ctrl_channel_worker_->SynchronizeNodeLayers();

    // synchronize cache block num.
    ctrl_channel_worker_->SynchronizeCacheBlockNum();

    // Disconnect from master.
    ctrl_channel_worker_->Disconnect();

    PipelineConfig pipeline_config;
    worker_env_->GetPipelineConfig(pipeline_config);
    worker_device_block_num = pipeline_config.device_block_num;
    worker_host_block_num = pipeline_config.host_block_num;

    worker_lower_layer_idx = pipeline_config.lower_layer_idx;
    worker_upper_layer_idx = pipeline_config.upper_layer_idx;
  };
  std::thread worker_thread = std::thread(worker_fn);

  master_thread.join();
  worker_thread.join();

  // Check layer range.
  EXPECT_EQ(master_lower_layer_idx, 0);
  EXPECT_EQ(master_upper_layer_idx, 15);
  EXPECT_EQ(worker_lower_layer_idx, 16);
  EXPECT_EQ(worker_upper_layer_idx, 31);

  // Check block num result.
  EXPECT_EQ(master_device_block_num, 6);
  EXPECT_EQ(master_host_block_num, 4);
  EXPECT_EQ(worker_device_block_num, 6);
  EXPECT_EQ(worker_host_block_num, 4);
}
