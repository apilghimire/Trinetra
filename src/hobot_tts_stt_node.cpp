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

#include "hobot_tts_stt_node.h"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace hobot_tts {

HobotTTSSTTNode::HobotTTSSTTNode(const rclcpp::NodeOptions& options)
  : Node("hobot_tts_stt_node", options), audio_manager_(nullptr), tts_engine_(nullptr),
    stt_engine_(nullptr), microphone_(nullptr), voice_assistant_enabled_(false) {
  
  InitializeParameters();
  InitializePublishersAndSubscribers();
  InitializeTTSEngine();
  InitializeSTTEngine();
  InitializeAudioManager();
  InitializeMicrophone();
  
  // Start the processing threads
  tts_thread_ = std::thread(&HobotTTSSTTNode::ProcessTTSMessages, this);
  stt_thread_ = std::thread(&HobotTTSSTTNode::ProcessSTTAudio, this);
  
  if (voice_assistant_enabled_) {
    StartVoiceAssistant();
  }
  
  RCLCPP_INFO(this->get_logger(), "Hobot TTS+STT Node initialized successfully");
}

HobotTTSSTTNode::~HobotTTSSTTNode() {
  shutdown_ = true;
  
  StopVoiceAssistant();
  
  // Stop processing threads
  if (tts_thread_.joinable()) {
    tts_thread_.join();
  }
  if (stt_thread_.joinable()) {
    stt_thread_.join();
  }
  
  // Clean up engines
  if (stt_engine_) {
    stt_engine_->StopRealTimeRecognition();
  }
  if (microphone_) {
    microphone_->StopRecording();
  }
}

void HobotTTSSTTNode::InitializeParameters() {
  // TTS Parameters
  this->declare_parameter("tts_engine", "say");
  this->declare_parameter("voice", "Alex");
  this->declare_parameter("audio_format", "wav");
  this->declare_parameter("sample_rate", 16000);
  this->declare_parameter("output_file", "/tmp/hobot_tts_output.wav");
  
  // STT Parameters
  this->declare_parameter("stt_engine", "whisper");
  this->declare_parameter("whisper_model", "tiny");
  this->declare_parameter("whisper_language", "auto");
  this->declare_parameter("stt_confidence_threshold", 0.7);
  
  // Recording Parameters
  this->declare_parameter("recording_sample_rate", 16000);
  this->declare_parameter("recording_channels", 1);
  this->declare_parameter("vad_enabled", true);
  this->declare_parameter("vad_silence_threshold", 0.01);
  this->declare_parameter("vad_min_speech_duration", 0.5);
  this->declare_parameter("vad_max_silence_duration", 1.0);
  
  // Voice Assistant Parameters
  this->declare_parameter("voice_assistant_enabled", false);
  this->declare_parameter("wake_word", "hello robot");
  this->declare_parameter("response_voice", "Samantha");
  
  // Get parameter values
  tts_engine_name_ = this->get_parameter("tts_engine").as_string();
  voice_ = this->get_parameter("voice").as_string();
  stt_engine_name_ = this->get_parameter("stt_engine").as_string();
  whisper_model_ = this->get_parameter("whisper_model").as_string();
  whisper_language_ = this->get_parameter("whisper_language").as_string();
  voice_assistant_enabled_ = this->get_parameter("voice_assistant_enabled").as_bool();
  wake_word_ = this->get_parameter("wake_word").as_string();
  response_voice_ = this->get_parameter("response_voice").as_string();
  
  RCLCPP_INFO(this->get_logger(), "TTS Engine: %s, Voice: %s", tts_engine_name_.c_str(), voice_.c_str());
  RCLCPP_INFO(this->get_logger(), "STT Engine: %s, Model: %s", stt_engine_name_.c_str(), whisper_model_.c_str());
}

void HobotTTSSTTNode::InitializePublishersAndSubscribers() {
  // TTS Subscriber
  tts_subscription_ = this->create_subscription<std_msgs::msg::String>(
    "tts_text", 10,
    std::bind(&HobotTTSSTTNode::TTSMessageCallback, this, std::placeholders::_1));
  
  // STT Publishers
  stt_result_publisher_ = this->create_publisher<std_msgs::msg::String>("stt_result", 10);
  stt_confidence_publisher_ = this->create_publisher<std_msgs::msg::Float32>("stt_confidence", 10);
  
  // Voice Assistant Publishers
  va_listening_publisher_ = this->create_publisher<std_msgs::msg::Bool>("va_listening", 10);
  va_speaking_publisher_ = this->create_publisher<std_msgs::msg::Bool>("va_speaking", 10);
  
  // Recording control service
  recording_service_ = this->create_service<std_srvs::srv::SetBool>(
    "control_recording",
    std::bind(&HobotTTSSTTNode::ControlRecordingCallback, this, 
              std::placeholders::_1, std::placeholders::_2));
  
  // Voice assistant control service
  va_service_ = this->create_service<std_srvs::srv::SetBool>(
    "control_voice_assistant",
    std::bind(&HobotTTSSTTNode::ControlVoiceAssistantCallback, this, 
              std::placeholders::_1, std::placeholders::_2));
}

void HobotTTSSTTNode::InitializeTTSEngine() {
  TTSEngineType engine_type;
  if (tts_engine_name_ == "say") {
    engine_type = TTSEngineType::SAY;
  } else if (tts_engine_name_ == "festival") {
    engine_type = TTSEngineType::FESTIVAL;
  } else if (tts_engine_name_ == "espeak") {
    engine_type = TTSEngineType::ESPEAK;
  } else {
    RCLCPP_WARN(this->get_logger(), "Unknown TTS engine: %s, using 'say'", tts_engine_name_.c_str());
    engine_type = TTSEngineType::SAY;
  }
  
  tts_engine_ = TTSEngine::Create(engine_type);
  if (!tts_engine_ || !tts_engine_->Initialize()) {
    RCLCPP_ERROR(this->get_logger(), "Failed to initialize TTS engine");
    return;
  }
  
  tts_engine_->SetVoice(voice_);
  RCLCPP_INFO(this->get_logger(), "TTS Engine initialized with voice: %s", voice_.c_str());
}

void HobotTTSSTTNode::InitializeSTTEngine() {
  STTEngineType engine_type;
  
  if (stt_engine_name_ == "whisper") {
    if (whisper_model_ == "tiny") {
      engine_type = STTEngineType::WHISPER_TINY;
    } else if (whisper_model_ == "base") {
      engine_type = STTEngineType::WHISPER_BASE;
    } else if (whisper_model_ == "small") {
      engine_type = STTEngineType::WHISPER_SMALL;
    } else {
      RCLCPP_WARN(this->get_logger(), "Unknown whisper model: %s, using 'tiny'", whisper_model_.c_str());
      engine_type = STTEngineType::WHISPER_TINY;
    }
  } else if (stt_engine_name_ == "macos") {
    engine_type = STTEngineType::MACOS_DICTATION;
  } else {
    RCLCPP_WARN(this->get_logger(), "Unknown STT engine: %s, using whisper tiny", stt_engine_name_.c_str());
    engine_type = STTEngineType::WHISPER_TINY;
  }
  
  stt_engine_ = STTEngine::Create(engine_type);
  if (!stt_engine_ || !stt_engine_->Initialize()) {
    RCLCPP_ERROR(this->get_logger(), "Failed to initialize STT engine");
    return;
  }
  
  if (whisper_language_ != "auto") {
    stt_engine_->SetLanguage(whisper_language_);
  }
  
  RCLCPP_INFO(this->get_logger(), "STT Engine initialized: %s", stt_engine_->GetModelInfo().c_str());
}

void HobotTTSSTTNode::InitializeAudioManager() {
  audio_manager_ = AudioManager::Create(AudioManagerType::CORE_AUDIO);
  if (!audio_manager_ || !audio_manager_->Initialize()) {
    RCLCPP_WARN(this->get_logger(), "CoreAudio failed, falling back to simple audio");
    audio_manager_ = AudioManager::Create(AudioManagerType::SIMPLE);
    if (!audio_manager_ || !audio_manager_->Initialize()) {
      RCLCPP_ERROR(this->get_logger(), "Failed to initialize any audio manager");
      return;
    }
  }
  
  RCLCPP_INFO(this->get_logger(), "Audio manager initialized");
}

void HobotTTSSTTNode::InitializeMicrophone() {
  microphone_ = std::make_unique<MicrophoneRecorder>();
  if (!microphone_->Initialize()) {
    RCLCPP_ERROR(this->get_logger(), "Failed to initialize microphone");
    return;
  }
  
  // Set recording parameters
  int sample_rate = this->get_parameter("recording_sample_rate").as_int();
  int channels = this->get_parameter("recording_channels").as_int();
  bool vad_enabled = this->get_parameter("vad_enabled").as_bool();
  double silence_threshold = this->get_parameter("vad_silence_threshold").as_double();
  double min_speech = this->get_parameter("vad_min_speech_duration").as_double();
  double max_silence = this->get_parameter("vad_max_silence_duration").as_double();
  
  microphone_->SetRecordingParameters(sample_rate, channels, 16);
  microphone_->SetVADParameters(vad_enabled, static_cast<float>(silence_threshold), 
                               static_cast<float>(min_speech), static_cast<float>(max_silence));
  
  RCLCPP_INFO(this->get_logger(), "Microphone initialized (%dkHz, %dch, VAD: %s)", 
              sample_rate/1000, channels, vad_enabled ? "enabled" : "disabled");
}

void HobotTTSSTTNode::TTSMessageCallback(const std_msgs::msg::String::SharedPtr msg) {
  std::lock_guard<std::mutex> lock(tts_queue_mutex_);
  tts_message_queue_.push(msg->data);
  RCLCPP_INFO(this->get_logger(), "Received TTS request: %s", msg->data.c_str());
}

void HobotTTSSTTNode::ProcessTTSMessages() {
  while (!shutdown_) {
    std::string text;
    
    {
      std::lock_guard<std::mutex> lock(tts_queue_mutex_);
      if (!tts_message_queue_.empty()) {
        text = tts_message_queue_.front();
        tts_message_queue_.pop();
      }
    }
    
    if (!text.empty()) {
      ProcessTTSText(text);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void HobotTTSSTTNode::ProcessSTTAudio() {
  while (!shutdown_) {
    AudioInput audio;
    
    {
      std::lock_guard<std::mutex> lock(stt_queue_mutex_);
      if (!stt_audio_queue_.empty()) {
        audio = stt_audio_queue_.front();
        stt_audio_queue_.pop();
      }
    }
    
    if (!audio.samples.empty()) {
      ProcessSTTAudio(audio);
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}

void HobotTTSSTTNode::ProcessTTSText(const std::string& text) {
  if (!tts_engine_ || !audio_manager_) {
    RCLCPP_ERROR(this->get_logger(), "TTS engine or audio manager not initialized");
    return;
  }
  
  // Publish speaking status
  auto speaking_msg = std_msgs::msg::Bool();
  speaking_msg.data = true;
  va_speaking_publisher_->publish(speaking_msg);
  
  try {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Generate audio
    TTSResult result = tts_engine_->Synthesize(text);
    
    if (result.success && !result.audio_file.empty()) {
      // Play audio
      bool played = audio_manager_->PlayAudio(result.audio_file);
      
      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
      
      if (played) {
        RCLCPP_INFO(this->get_logger(), "✓ TTS completed in %ldms: '%s'", duration.count(), text.c_str());
      } else {
        RCLCPP_ERROR(this->get_logger(), "Failed to play TTS audio");
      }
    } else {
      RCLCPP_ERROR(this->get_logger(), "TTS synthesis failed");
    }
    
  } catch (const std::exception& e) {
    RCLCPP_ERROR(this->get_logger(), "TTS processing error: %s", e.what());
  }
  
  // Publish speaking status
  speaking_msg.data = false;
  va_speaking_publisher_->publish(speaking_msg);
}

void HobotTTSSTTNode::ProcessSTTAudio(const AudioInput& audio) {
  if (!stt_engine_) {
    RCLCPP_ERROR(this->get_logger(), "STT engine not initialized");
    return;
  }
  
  try {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    STTResult result = stt_engine_->ProcessAudio(audio);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (result.success) {
      // Publish STT result
      auto text_msg = std_msgs::msg::String();
      text_msg.data = result.text;
      stt_result_publisher_->publish(text_msg);
      
      auto confidence_msg = std_msgs::msg::Float32();
      confidence_msg.data = result.confidence;
      stt_confidence_publisher_->publish(confidence_msg);
      
      RCLCPP_INFO(this->get_logger(), "✓ STT result (%.1fs, conf=%.2f): '%s'", 
                  duration.count()/1000.0f, result.confidence, result.text.c_str());
      
      // Handle voice assistant interaction
      if (voice_assistant_enabled_) {
        HandleVoiceAssistantInput(result.text);
      }
      
    } else {
      RCLCPP_WARN(this->get_logger(), "STT processing failed");
    }
    
  } catch (const std::exception& e) {
    RCLCPP_ERROR(this->get_logger(), "STT processing error: %s", e.what());
  }
}

void HobotTTSSTTNode::StartVoiceAssistant() {
  if (!microphone_ || voice_assistant_active_) {
    return;
  }
  
  voice_assistant_active_ = true;
  
  va_thread_ = std::thread([this]() {
    RCLCPP_INFO(this->get_logger(), "🤖 Voice Assistant started (wake word: '%s')", wake_word_.c_str());
    
    while (voice_assistant_active_ && !shutdown_) {
      // Publish listening status
      auto listening_msg = std_msgs::msg::Bool();
      listening_msg.data = true;
      va_listening_publisher_->publish(listening_msg);
      
      // Record audio with VAD
      AudioInput audio = microphone_->RecordWithVAD();
      
      listening_msg.data = false;
      va_listening_publisher_->publish(listening_msg);
      
      if (!audio.samples.empty()) {
        // Queue audio for STT processing
        std::lock_guard<std::mutex> lock(stt_queue_mutex_);
        stt_audio_queue_.push(audio);
      }
      
      // Small delay before next recording
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
  });
}

void HobotTTSSTTNode::StopVoiceAssistant() {
  voice_assistant_active_ = false;
  if (va_thread_.joinable()) {
    va_thread_.join();
  }
}

void HobotTTSSTTNode::HandleVoiceAssistantInput(const std::string& text) {
  // Simple wake word detection (case insensitive)
  std::string lower_text = text;
  std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
  
  std::string lower_wake_word = wake_word_;
  std::transform(lower_wake_word.begin(), lower_wake_word.end(), lower_wake_word.begin(), ::tolower);
  
  if (lower_text.find(lower_wake_word) != std::string::npos) {
    RCLCPP_INFO(this->get_logger(), "🎤 Wake word detected: %s", text.c_str());
    
    // Extract command after wake word
    size_t pos = lower_text.find(lower_wake_word);
    if (pos != std::string::npos) {
      std::string command = text.substr(pos + wake_word_.length());
      
      // Trim whitespace
      command.erase(0, command.find_first_not_of(" \t\n\r"));
      command.erase(command.find_last_not_of(" \t\n\r") + 1);
      
      if (!command.empty()) {
        HandleVoiceCommand(command);
      } else {
        // Just acknowledged wake word
        std::string response = "Yes, I'm listening. How can I help you?";
        QueueTTSResponse(response);
      }
    }
  }
}

void HobotTTSSTTNode::HandleVoiceCommand(const std::string& command) {
  std::string lower_command = command;
  std::transform(lower_command.begin(), lower_command.end(), lower_command.begin(), ::tolower);
  
  std::string response;
  
  // Simple command processing
  if (lower_command.find("hello") != std::string::npos) {
    response = "Hello! I'm the Hobot TTS and speech recognition system.";
  } else if (lower_command.find("time") != std::string::npos) {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    std::ostringstream oss;
    oss << "The current time is " << std::put_time(tm, "%I:%M %p");
    response = oss.str();
  } else if (lower_command.find("weather") != std::string::npos) {
    response = "I don't have access to weather information yet, but it's always a good day for robotics!";
  } else if (lower_command.find("stop") != std::string::npos || lower_command.find("quit") != std::string::npos) {
    response = "Goodbye!";
    QueueTTSResponse(response);
    // Stop voice assistant after response
    std::this_thread::sleep_for(std::chrono::seconds(3));
    voice_assistant_active_ = false;
    return;
  } else {
    response = "I heard you say: " + command + ". I'm still learning how to respond to different commands.";
  }
  
  QueueTTSResponse(response);
}

void HobotTTSSTTNode::QueueTTSResponse(const std::string& response) {
  // Temporarily change voice for responses
  if (tts_engine_ && response_voice_ != voice_) {
    tts_engine_->SetVoice(response_voice_);
  }
  
  std::lock_guard<std::mutex> lock(tts_queue_mutex_);
  tts_message_queue_.push(response);
  
  // Restore original voice after queuing
  if (tts_engine_ && response_voice_ != voice_) {
    // We'll restore it after processing, but for now just note the difference
  }
}

void HobotTTSSTTNode::ControlRecordingCallback(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
    std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
  
  if (!microphone_) {
    response->success = false;
    response->message = "Microphone not initialized";
    return;
  }
  
  if (request->data) {
    // Start recording
    bool started = microphone_->StartContinuousRecording([this](const AudioInput& audio) {
      std::lock_guard<std::mutex> lock(stt_queue_mutex_);
      stt_audio_queue_.push(audio);
    });
    
    response->success = started;
    response->message = started ? "Recording started" : "Failed to start recording";
  } else {
    // Stop recording
    microphone_->StopContinuousRecording();
    response->success = true;
    response->message = "Recording stopped";
  }
}

void HobotTTSSTTNode::ControlVoiceAssistantCallback(
    const std::shared_ptr<std_srvs::srv::SetBool::Request> request,
    std::shared_ptr<std_srvs::srv::SetBool::Response> response) {
  
  if (request->data) {
    if (!voice_assistant_active_) {
      StartVoiceAssistant();
      response->success = true;
      response->message = "Voice assistant started";
    } else {
      response->success = false;
      response->message = "Voice assistant already active";
    }
  } else {
    if (voice_assistant_active_) {
      StopVoiceAssistant();
      response->success = true;
      response->message = "Voice assistant stopped";
    } else {
      response->success = false;
      response->message = "Voice assistant not active";
    }
  }
}

}  // namespace hobot_tts
