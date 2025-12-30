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

#include "hobot_tts_node.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace hobot_tts {

HobotTTSNode::HobotTTSNode(rclcpp::Node::SharedPtr& nh) : nh_(nh) {
  // Declare and get parameters
  nh_->declare_parameter<std::string>("topic_sub", topic_subscription_name_);
  nh_->declare_parameter<std::string>("audio_device", audio_device_name_);
  nh_->declare_parameter<std::string>("tts_engine", tts_engine_name_);
  nh_->declare_parameter<std::string>("voice_name", voice_name_);
  
  nh_->get_parameter<std::string>("topic_sub", topic_subscription_name_);
  nh_->get_parameter<std::string>("audio_device", audio_device_name_);
  nh_->get_parameter<std::string>("tts_engine", tts_engine_name_);
  nh_->get_parameter<std::string>("voice_name", voice_name_);

  RCLCPP_INFO(nh_->get_logger(), "Initializing Hobot TTS Node");
  RCLCPP_INFO(nh_->get_logger(), "Topic: %s", topic_subscription_name_.c_str());
  RCLCPP_INFO(nh_->get_logger(), "Audio Device: %s", audio_device_name_.c_str());
  RCLCPP_INFO(nh_->get_logger(), "TTS Engine: %s", tts_engine_name_.c_str());
  RCLCPP_INFO(nh_->get_logger(), "Voice: %s", voice_name_.c_str());

  // Initialize TTS engine
  TTSEngineType engine_type = TTSEngineType::SAY;
  if (tts_engine_name_ == "festival") {
    engine_type = TTSEngineType::FESTIVAL;
  } else if (tts_engine_name_ == "espeak") {
    engine_type = TTSEngineType::ESPEAK;
  }

  tts_engine_ = TTSEngine::Create(engine_type, voice_name_);
  if (!tts_engine_ || !tts_engine_->Initialize()) {
    throw std::runtime_error("Failed to initialize TTS engine");
  }

  // Initialize audio manager
  audio_manager_ = AudioManager::Create(audio_device_name_);
  if (!audio_manager_ || !audio_manager_->Initialize()) {
    throw std::runtime_error("Failed to initialize audio manager");
  }

  // Create subscription
  text_subscription_ = nh_->create_subscription<std_msgs::msg::String>(
    topic_subscription_name_, 10,
    std::bind(&HobotTTSNode::MessageCallback, this, std::placeholders::_1));

  // Start processing threads
  processing_thread_ = std::thread(&HobotTTSNode::ProcessMessages, this);
  playback_thread_ = std::thread(&HobotTTSNode::PlaybackMessages, this);

  // List available voices for information
  auto voices = tts_engine_->GetAvailableVoices();
  if (!voices.empty()) {
    RCLCPP_INFO(nh_->get_logger(), "Available voices:");
    for (const auto& voice : voices) {
      RCLCPP_INFO(nh_->get_logger(), "  - %s", voice.c_str());
    }
  }

  RCLCPP_INFO(nh_->get_logger(), "Hobot TTS Node initialized successfully");
}

HobotTTSNode::~HobotTTSNode() {
  StopProcessing();
  
  if (tts_engine_) {
    tts_engine_->Cleanup();
  }
  
  if (audio_manager_) {
    audio_manager_->Cleanup();
  }
}

void HobotTTSNode::MessageCallback(const std_msgs::msg::String::SharedPtr msg) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (message_queue_.size() >= kMaxMessageQueueSize) {
    // Discard the oldest message if the queue size exceeds the limit
    message_queue_.pop();
    RCLCPP_WARN(nh_->get_logger(), "Message queue full, discarding oldest message");
  }
  
  message_queue_.push(msg);
  cv_.notify_one();
  
  RCLCPP_DEBUG(nh_->get_logger(), "Received text message: %s", msg->data.c_str());
}

void HobotTTSNode::ProcessMessages() {
  while (rclcpp::ok()) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return !message_queue_.empty() || stop_processing_; });

    if (stop_processing_ && message_queue_.empty()) {
      break;
    }

    while (!message_queue_.empty()) {
      auto message = message_queue_.front();
      message_queue_.pop();
      lock.unlock();

      // Split text into smaller segments
      auto segments = SplitText(message->data);
      
      for (const auto& segment : segments) {
        if (segment.empty()) continue;

        RCLCPP_INFO(nh_->get_logger(), "Processing text: \"%s\"", segment.c_str());

        // Convert text to speech
        AudioData audio_data;
        if (tts_engine_->Synthesize(segment, audio_data)) {
          // Add to audio queue
          std::lock_guard<std::mutex> audio_lock(audio_mutex_);
          if (audio_queue_.size() >= kMaxAudioQueueSize) {
            audio_queue_.pop();
            RCLCPP_WARN(nh_->get_logger(), "Audio queue full, discarding oldest audio");
          }
          audio_queue_.push(audio_data.samples);
          cv_audio_.notify_one();
        } else {
          RCLCPP_ERROR(nh_->get_logger(), "Failed to synthesize text: %s", segment.c_str());
        }
      }

      lock.lock();
    }
  }
}

void HobotTTSNode::PlaybackMessages() {
  while (rclcpp::ok()) {
    std::unique_lock<std::mutex> lock(audio_mutex_);
    cv_audio_.wait(lock, [this] { return !audio_queue_.empty() || stop_processing_; });

    if (stop_processing_ && audio_queue_.empty()) {
      break;
    }

    while (!audio_queue_.empty()) {
      auto audio_data = audio_queue_.front();
      audio_queue_.pop();
      lock.unlock();

      // Play audio
      if (!audio_manager_->PlayAudio(audio_data, 16000, 1)) {
        RCLCPP_ERROR(nh_->get_logger(), "Failed to play audio");
      }

      lock.lock();
    }
  }
}

void HobotTTSNode::StopProcessing() {
  if (!stop_processing_) {
    stop_processing_ = true;
    cv_.notify_one();
    cv_audio_.notify_one();
    
    if (processing_thread_.joinable()) {
      processing_thread_.join();
    }
    
    if (playback_thread_.joinable()) {
      playback_thread_.join();
    }
  }
}

std::vector<std::string> HobotTTSNode::SplitText(const std::string& text) {
  std::vector<std::string> segments;
  std::string current_segment;
  
  size_t start_pos = 0;
  size_t index = 0;
  
  // Process character by character
  while (index < text.length()) {
    // Check for Chinese punctuation
    if (IsChinesePunctuation(text, index)) {
      current_segment = text.substr(start_pos, index - start_pos);
      if (!current_segment.empty()) {
        segments.push_back(current_segment);
      }
      start_pos = index + 3; // Chinese characters are 3 bytes in UTF-8
      index += 3;
      continue;
    }
    
    // Check for English punctuation
    if (std::ispunct(text[index])) {
      current_segment = text.substr(start_pos, index - start_pos);
      if (!current_segment.empty()) {
        segments.push_back(current_segment);
      }
      start_pos = index + 1;
    }
    
    // Check for whitespace
    if (std::isspace(text[index])) {
      current_segment = text.substr(start_pos, index - start_pos);
      if (!current_segment.empty()) {
        segments.push_back(current_segment);
      }
      start_pos = index + 1;
    }
    
    ++index;
  }
  
  // Add remaining text
  current_segment = text.substr(start_pos);
  if (!current_segment.empty()) {
    segments.push_back(current_segment);
  }
  
  // If no segments found, add the whole text
  if (segments.empty() && !text.empty()) {
    segments.push_back(text);
  }
  
  return segments;
}

bool HobotTTSNode::IsChinesePunctuation(const std::string& str, size_t index) {
  if (index + 2 >= str.length()) return false;
  
  // Check for common Chinese punctuation marks in UTF-8
  return (str[index] == '\xEF' && str[index + 1] == '\xBC' &&
          (str[index + 2] == '\x8C' || str[index + 2] == '\x9F' ||  // ，。
           str[index + 2] == '\x9A' || str[index + 2] == '\x81')) || // ？！
         (str[index] == '\xE3' && str[index + 1] == '\x80' &&
          (str[index + 2] == '\x82' || str[index + 2] == '\x81'));   // 、。
}

}  // namespace hobot_tts
