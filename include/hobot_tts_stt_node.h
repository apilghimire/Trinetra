// Copyright (c) 2024, Extended Hobot TTS with Speech-to-Text.
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

#ifndef HOBOT_TTS_STT_NODE_H_
#define HOBOT_TTS_STT_NODE_H_

#include <rclcpp/rclcpp.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/bool.hpp>
#include <std_msgs/msg/float32.hpp>
#include <std_srvs/srv/set_bool.hpp>

#include "tts_engine.h"
#include "stt_engine.h"
#include "audio_manager.h"
#include "microphone_recorder.h"

#include <queue>
#include <mutex>
#include <thread>
#include <atomic>
#include <memory>
#include <algorithm>
#include <cctype>

namespace hobot_tts {

/**
 * @class HobotTTSSTTNode
 * @brief ROS2 node providing both Text-to-Speech and Speech-to-Text capabilities
 * 
 * This node combines TTS and STT functionality into a unified voice interface.
 * Features include:
 * - Text-to-speech synthesis with multiple engines and voices
 * - Speech-to-text recognition with Whisper models
 * - Voice assistant mode with wake word detection
 * - Microphone recording with Voice Activity Detection
 * - Real-time audio processing and playback
 */
class HobotTTSSTTNode : public rclcpp::Node {
public:
  /**
   * @brief Constructor
   * @param options ROS2 node options
   */
  explicit HobotTTSSTTNode(const rclcpp::NodeOptions& options = rclcpp::NodeOptions());
  
  /**
   * @brief Destructor - cleans up threads and engines
   */
  ~HobotTTSSTTNode();

private:
  // Core engines and managers
  std::unique_ptr<AudioManager> audio_manager_;
  std::unique_ptr<TTSEngine> tts_engine_;
  std::unique_ptr<STTEngine> stt_engine_;
  std::unique_ptr<MicrophoneRecorder> microphone_;
  
  // ROS2 interfaces
  rclcpp::Subscription<std_msgs::msg::String>::SharedPtr tts_subscription_;
  rclcpp::Publisher<std_msgs::msg::String>::SharedPtr stt_result_publisher_;
  rclcpp::Publisher<std_msgs::msg::Float32>::SharedPtr stt_confidence_publisher_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr va_listening_publisher_;
  rclcpp::Publisher<std_msgs::msg::Bool>::SharedPtr va_speaking_publisher_;
  
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr recording_service_;
  rclcpp::Service<std_srvs::srv::SetBool>::SharedPtr va_service_;
  
  // Processing threads
  std::thread tts_thread_;
  std::thread stt_thread_;
  std::thread va_thread_;
  std::atomic<bool> shutdown_{false};
  
  // Message queues
  std::queue<std::string> tts_message_queue_;
  std::queue<AudioInput> stt_audio_queue_;
  std::mutex tts_queue_mutex_;
  std::mutex stt_queue_mutex_;
  
  // Configuration parameters
  std::string tts_engine_name_;
  std::string voice_;
  std::string stt_engine_name_;
  std::string whisper_model_;
  std::string whisper_language_;
  
  // Voice assistant parameters
  bool voice_assistant_enabled_;
  std::atomic<bool> voice_assistant_active_{false};
  std::string wake_word_;
  std::string response_voice_;
  
  /**
   * @brief Initialize ROS2 parameters from parameter server
   */
  void InitializeParameters();
  
  /**
   * @brief Set up ROS2 publishers, subscribers, and services
   */
  void InitializePublishersAndSubscribers();
  
  /**
   * @brief Initialize the TTS engine with configured parameters
   */
  void InitializeTTSEngine();
  
  /**
   * @brief Initialize the STT engine with configured parameters
   */
  void InitializeSTTEngine();
  
  /**
   * @brief Initialize the audio manager for playback
   */
  void InitializeAudioManager();
  
  /**
   * @brief Initialize the microphone recorder
   */
  void InitializeMicrophone();
  
  /**
   * @brief Callback for incoming TTS text messages
   * @param msg String message containing text to synthesize
   */
  void TTSMessageCallback(const std_msgs::msg::String::SharedPtr msg);
  
  /**
   * @brief Thread function to process TTS message queue
   */
  void ProcessTTSMessages();
  
  /**
   * @brief Thread function to process STT audio queue
   */
  void ProcessSTTAudio();
  
  /**
   * @brief Process a single TTS text request
   * @param text Text to synthesize and play
   */
  void ProcessTTSText(const std::string& text);
  
  /**
   * @brief Process a single STT audio input
   * @param audio Audio input to recognize
   */
  void ProcessSTTAudio(const AudioInput& audio);
  
  /**
   * @brief Start the voice assistant mode
   */
  void StartVoiceAssistant();
  
  /**
   * @brief Stop the voice assistant mode
   */
  void StopVoiceAssistant();
  
  /**
   * @brief Handle voice assistant input text
   * @param text Recognized speech text
   */
  void HandleVoiceAssistantInput(const std::string& text);
  
  /**
   * @brief Process voice commands after wake word detection
   * @param command Command text to process
   */
  void HandleVoiceCommand(const std::string& command);
  
  /**
   * @brief Queue a TTS response for voice assistant
   * @param response Text to speak as response
   */
  void QueueTTSResponse(const std::string& response);
  
  /**
   * @brief Service callback to control recording
   * @param request Service request (enable/disable)
   * @param response Service response (success/failure)
   */
  void ControlRecordingCallback(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
    std::shared_ptr<std_srvs::srv::SetBool::Response> response);
  
  /**
   * @brief Service callback to control voice assistant
   * @param request Service request (enable/disable)
   * @param response Service response (success/failure)
   */
  void ControlVoiceAssistantCallback(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
    std::shared_ptr<std_srvs::srv::SetBool::Response> response);
};

}  // namespace hobot_tts

#endif  // HOBOT_TTS_STT_NODE_H_
