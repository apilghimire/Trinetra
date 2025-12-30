// Copyright (c) 2024, Adapted from D-Robotics Hobot TTS.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef HOBOT_TTS_MACOS_HOBOT_TTS_NODE_H_
#define HOBOT_TTS_MACOS_HOBOT_TTS_NODE_H_

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "std_msgs/msg/string.hpp"
#include "tts_engine.h"
#include "audio_manager.h"

namespace hobot_tts {

class HobotTTSNode {
 public:
  HobotTTSNode(rclcpp::Node::SharedPtr& nh);
  ~HobotTTSNode();

 private:
  rclcpp::Node::SharedPtr nh_;
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr text_subscription_;
  
  // Parameters
  std::string topic_subscription_name_ = "/tts_text";
  std::string audio_device_name_ = "default";
  std::string tts_engine_name_ = "say";
  std::string voice_name_ = "default";

  // Core components
  std::unique_ptr<TTSEngine> tts_engine_;
  std::unique_ptr<AudioManager> audio_manager_;

  // Message processing
  void MessageCallback(const std_msgs::msg::String::SharedPtr msg);
  void ProcessMessages();
  void PlaybackMessages();
  void StopProcessing();

  // Text processing utilities
  std::vector<std::string> SplitText(const std::string& text);
  bool IsChinesePunctuation(const std::string& str, size_t index);

  // Thread management
  std::queue<std_msgs::msg::String::SharedPtr> message_queue_;
  std::mutex mutex_;
  std::condition_variable cv_;

  std::queue<std::vector<int16_t>> audio_queue_;
  std::mutex audio_mutex_;
  std::condition_variable cv_audio_;

  std::atomic<bool> stop_processing_{false};
  std::thread processing_thread_;
  std::thread playback_thread_;

  static constexpr size_t kMaxMessageQueueSize = 10;
  static constexpr size_t kMaxAudioQueueSize = 5;
};

}  // namespace hobot_tts

#endif  // HOBOT_TTS_MACOS_HOBOT_TTS_NODE_H_
