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

#include "stt_engine.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <sstream>
#include <cmath>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#endif

// C++14 compatible file operations
namespace {
  bool remove_file(const std::string& filename) {
    return std::remove(filename.c_str()) == 0;
  }
  
  bool file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
  }
}

namespace hobot_tts {

// STTEngine factory method
std::unique_ptr<STTEngine> STTEngine::Create(STTEngineType type, const std::string& model_path) {
  switch (type) {
    case STTEngineType::WHISPER_TINY:
    case STTEngineType::WHISPER_BASE:
    case STTEngineType::WHISPER_SMALL:
      return std::make_unique<WhisperSTTEngine>(type, model_path);
    case STTEngineType::WHISPER_API:
      // For API, model_path should contain the API key
      return std::make_unique<WhisperAPIEngine>(model_path);
    case STTEngineType::MACOS_DICTATION:
      return std::make_unique<MacOSDictationEngine>();
    default:
      return std::make_unique<WhisperSTTEngine>(STTEngineType::WHISPER_TINY, model_path);
  }
}

// Factory method with API key
std::unique_ptr<STTEngine> STTEngine::CreateWithAPIKey(const std::string& api_key, const std::string& model) {
  return std::make_unique<WhisperAPIEngine>(api_key, model);
}

// WhisperSTTEngine implementation
WhisperSTTEngine::WhisperSTTEngine(STTEngineType model_type, const std::string& model_path)
  : model_type_(model_type), model_path_(model_path), language_("auto"), 
    vad_enabled_(true), initialized_(false), real_time_active_(false), whisper_ctx_(nullptr) {
}

WhisperSTTEngine::~WhisperSTTEngine() {
  Cleanup();
}

bool WhisperSTTEngine::Initialize() {
  std::cout << "Initializing Whisper STT Engine (" << ModelTypeToString() << ")..." << std::endl;
  
  // Check if we need to download the model
  if (model_path_.empty()) {
    std::string home_dir = std::getenv("HOME");
    std::string models_dir = home_dir + "/.cache/whisper";
    std::system(("mkdir -p " + models_dir).c_str());
    model_path_ = models_dir + "/" + ModelTypeToString() + ".bin";
  }
  
  // Download model if it doesn't exist
  if (!file_exists(model_path_)) {
    std::cout << "Model not found, downloading..." << std::endl;
    if (!DownloadModel()) {
      std::cerr << "Failed to download Whisper model" << std::endl;
      return false;
    }
  }
  
  // For this implementation, we'll use a Python wrapper to whisper
  // In a production system, you'd use whisper.cpp directly
  std::cout << "✓ Whisper STT Engine initialized successfully" << std::endl;
  initialized_ = true;
  return true;
}

STTResult WhisperSTTEngine::ProcessAudio(const AudioInput& audio) {
  STTResult result;
  auto start_time = std::chrono::high_resolution_clock::now();
  
  if (!initialized_) {
    result.success = false;
    return result;
  }
  
  if (audio.samples.empty()) {
    result.success = false;
    return result;
  }
  
  try {
    // Save audio to temporary file
    std::string temp_file = "/tmp/whisper_input_" + std::to_string(std::rand()) + ".wav";
    
    // Create WAV file
    std::ofstream file(temp_file, std::ios::binary);
    if (!file.is_open()) {
      result.success = false;
      return result;
    }
    
    // Write WAV header
    const int data_size = audio.samples.size() * sizeof(int16_t);
    const int file_size = 36 + data_size;
    const int byte_rate = audio.sample_rate * audio.channels * 2;
    
    file.write("RIFF", 4);
    file.write(reinterpret_cast<const char*>(&file_size), 4);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    const int fmt_size = 16;
    file.write(reinterpret_cast<const char*>(&fmt_size), 4);
    const short audio_format = 1;
    file.write(reinterpret_cast<const char*>(&audio_format), 2);
    const short channels = audio.channels;
    file.write(reinterpret_cast<const char*>(&channels), 2);
    file.write(reinterpret_cast<const char*>(&audio.sample_rate), 4);
    file.write(reinterpret_cast<const char*>(&byte_rate), 4);
    const short block_align = audio.channels * 2;
    file.write(reinterpret_cast<const char*>(&block_align), 2);
    const short bits_per_sample = 16;
    file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
    file.write("data", 4);
    file.write(reinterpret_cast<const char*>(&data_size), 4);
    file.write(reinterpret_cast<const char*>(audio.samples.data()), data_size);
    file.close();
    
    // Run whisper (using Python implementation for simplicity)
    std::ostringstream cmd;
    cmd << "python3 -c \""
        << "import whisper; "
        << "import sys; "
        << "try: "
        << "  model = whisper.load_model('" << ModelTypeToString() << "'); "
        << "  result = model.transcribe('" << temp_file << "'"; 
    
    if (language_ != "auto") {
      cmd << ", language='" << language_ << "'";
    }
    
    cmd << "); "
        << "  print(result['text'].strip()); "
        << "except Exception as e: "
        << "  print('ERROR: ' + str(e), file=sys.stderr); "
        << "  sys.exit(1)"
        << "\" 2>/dev/null";
    
    // Execute whisper
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
      remove_file(temp_file);
      result.success = false;
      return result;
    }
    
    char buffer[4096];
    std::string output;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
      output += buffer;
    }
    
    int return_code = pclose(pipe);
    remove_file(temp_file);
    
    if (return_code == 0 && !output.empty()) {
      // Remove trailing newline
      if (output.back() == '\n') output.pop_back();
      
      result.text = output;
      result.success = true;
      result.confidence = 0.9f; // Whisper doesn't provide confidence scores easily
      result.language = (language_ == "auto") ? "en" : language_;
    } else {
      result.success = false;
    }
    
  } catch (const std::exception& e) {
    std::cerr << "Whisper processing error: " << e.what() << std::endl;
    result.success = false;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  result.processing_time_ms = duration.count();
  
  return result;
}

bool WhisperSTTEngine::StartRealTimeRecognition(STTCallback callback) {
  if (real_time_active_) {
    return false;
  }
  
  real_time_callback_ = callback;
  real_time_active_ = true;
  
  real_time_thread_ = std::thread([this]() {
    while (real_time_active_) {
      // Record 3 seconds of audio
      AudioInput audio = RecordAudio(3.0f);
      
      if (!audio.samples.empty()) {
        STTResult result = ProcessAudio(audio);
        if (result.success && !result.text.empty() && real_time_callback_) {
          real_time_callback_(result);
        }
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  
  return true;
}

void WhisperSTTEngine::StopRealTimeRecognition() {
  real_time_active_ = false;
  if (real_time_thread_.joinable()) {
    real_time_thread_.join();
  }
}

void WhisperSTTEngine::SetLanguage(const std::string& language) {
  language_ = language;
}

std::vector<std::string> WhisperSTTEngine::GetSupportedLanguages() {
  return {
    "auto", "en", "es", "fr", "de", "it", "pt", "ru", "ja", "ko", 
    "zh", "ar", "hi", "tr", "pl", "nl", "sv", "da", "no", "fi"
  };
}

void WhisperSTTEngine::SetVAD(bool enable) {
  vad_enabled_ = enable;
}

std::string WhisperSTTEngine::GetModelInfo() const {
  std::ostringstream info;
  info << "OpenAI Whisper " << ModelTypeToString();
  switch (model_type_) {
    case STTEngineType::WHISPER_TINY:
      info << " (39 MB, fastest, lowest accuracy)";
      break;
    case STTEngineType::WHISPER_BASE:
      info << " (74 MB, balanced speed/accuracy)";
      break;
    case STTEngineType::WHISPER_SMALL:
      info << " (244 MB, better accuracy)";
      break;
    default:
      break;
  }
  return info.str();
}

void WhisperSTTEngine::Cleanup() {
  StopRealTimeRecognition();
  initialized_ = false;
  whisper_ctx_ = nullptr;
}

bool WhisperSTTEngine::DownloadModel() {
  std::cout << "Downloading " << ModelTypeToString() << " model..." << std::endl;
  
  // Install whisper if not already installed
  std::system("pip3 install -q openai-whisper 2>/dev/null");
  
  // Download model using whisper command
  std::ostringstream cmd;
  cmd << "python3 -c \""
      << "import whisper; "
      << "print('Downloading " << ModelTypeToString() << " model...'); "
      << "whisper.load_model('" << ModelTypeToString() << "'); "
      << "print('✓ Model downloaded successfully')\""
      << " 2>/dev/null";
  
  int result = std::system(cmd.str().c_str());
  return result == 0;
}

std::string WhisperSTTEngine::ModelTypeToString() const {
  switch (model_type_) {
    case STTEngineType::WHISPER_TINY: return "tiny";
    case STTEngineType::WHISPER_BASE: return "base";
    case STTEngineType::WHISPER_SMALL: return "small";
    default: return "tiny";
  }
}

AudioInput WhisperSTTEngine::RecordAudio(float duration_seconds) {
  AudioInput audio;
  
  // Create temporary file for recording
  std::string temp_file = "/tmp/record_" + std::to_string(std::rand()) + ".wav";
  
  // Record audio using macOS
  std::ostringstream cmd;
  cmd << "rec -q -t wav -c 1 -r 16000 -b 16 \"" << temp_file << "\" trim 0 " << duration_seconds;
  
  // Fallback to ffmpeg if sox (rec) not available
  if (std::system("which rec > /dev/null 2>&1") != 0) {
    cmd.str("");
    cmd << "ffmpeg -y -loglevel quiet -f avfoundation -i \":0\" -t " << duration_seconds 
        << " -ar 16000 -ac 1 -sample_fmt s16 \"" << temp_file << "\" 2>/dev/null";
  }
  
  if (std::system(cmd.str().c_str()) == 0 && file_exists(temp_file)) {
    // Read the recorded file
    std::ifstream file(temp_file, std::ios::binary);
    if (file.is_open()) {
      // Skip WAV header
      file.seekg(44);
      
      // Read audio data
      int16_t sample;
      while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
        audio.samples.push_back(sample);
      }
      
      audio.sample_rate = 16000;
      audio.channels = 1;
      audio.duration_seconds = duration_seconds;
      
      file.close();
    }
    
    remove_file(temp_file);
  }
  
  return audio;
}

// MacOSDictationEngine implementation (simplified)
MacOSDictationEngine::MacOSDictationEngine() : initialized_(false), language_("en-US") {
}

MacOSDictationEngine::~MacOSDictationEngine() {
  Cleanup();
}

bool MacOSDictationEngine::Initialize() {
  // Check if dictation is available
  if (!IsDictationEnabled()) {
    std::cerr << "macOS Dictation not enabled. Enable in System Preferences > Keyboard > Dictation" << std::endl;
    return false;
  }
  
  initialized_ = true;
  return true;
}

STTResult MacOSDictationEngine::ProcessAudio(const AudioInput& audio) {
  STTResult result;
  
  if (!initialized_) {
    result.success = false;
    return result;
  }
  
  // This is a simplified implementation
  // Real macOS dictation integration would require Objective-C and Speech framework
  result.text = "[macOS Dictation - Placeholder Implementation]";
  result.success = false; // Mark as not implemented
  result.confidence = 0.0f;
  
  return result;
}

bool MacOSDictationEngine::StartRealTimeRecognition(STTCallback callback) {
  return false; // Not supported in this implementation
}

void MacOSDictationEngine::StopRealTimeRecognition() {
  // Not implemented
}

void MacOSDictationEngine::SetLanguage(const std::string& language) {
  language_ = language;
}

std::vector<std::string> MacOSDictationEngine::GetSupportedLanguages() {
  return {"en-US", "en-GB", "es-ES", "fr-FR", "de-DE", "it-IT", "ja-JP", "ko-KR", "zh-CN"};
}

void MacOSDictationEngine::SetVAD(bool enable) {
  // Not configurable for macOS dictation
}

void MacOSDictationEngine::Cleanup() {
  initialized_ = false;
}

bool MacOSDictationEngine::IsDictationEnabled() {
  // Simplified check - in real implementation, would check system preferences
  return std::system("which say > /dev/null 2>&1") == 0; // Basic macOS check
}

// WhisperAPIEngine implementation
WhisperAPIEngine::WhisperAPIEngine(const std::string& api_key, const std::string& model)
  : api_key_(api_key), model_(model), language_("auto"), 
    vad_enabled_(true), initialized_(false), real_time_active_(false) {
}

WhisperAPIEngine::~WhisperAPIEngine() {
  Cleanup();
}

bool WhisperAPIEngine::Initialize() {
  std::cout << "Initializing OpenAI Whisper API Engine..." << std::endl;
  
  if (api_key_.empty()) {
    std::cerr << "No API key provided for OpenAI Whisper API" << std::endl;
    return false;
  }
  
  // Check if curl is available for API requests
  if (std::system("which curl > /dev/null 2>&1") != 0) {
    std::cerr << "curl not found - required for OpenAI API requests" << std::endl;
    std::cerr << "Install with: brew install curl" << std::endl;
    return false;
  }
  
  // Test API connection with a simple request (optional)
  std::cout << "✓ OpenAI Whisper API Engine initialized (model: " << model_ << ")" << std::endl;
  initialized_ = true;
  return true;
}

STTResult WhisperAPIEngine::ProcessAudio(const AudioInput& audio) {
  STTResult result;
  auto start_time = std::chrono::high_resolution_clock::now();
  
  if (!initialized_) {
    result.success = false;
    return result;
  }
  
  if (audio.samples.empty()) {
    result.success = false;
    return result;
  }
  
  try {
    // Save audio to temporary file (MP3 format for API)
    std::string audio_file = SaveAudioToFile(audio, "mp3");
    if (audio_file.empty()) {
      result.success = false;
      return result;
    }
    
    // Make API request
    std::string response = MakeAPIRequest(audio_file);
    
    // Clean up temporary file
    remove_file(audio_file);
    
    if (!response.empty()) {
      // Parse API response
      result.text = ParseAPIResponse(response);
      result.success = !result.text.empty();
      result.confidence = 0.95f; // OpenAI API doesn't provide confidence scores
      result.language = (language_ == "auto") ? "en" : language_;
    } else {
      result.success = false;
    }
    
  } catch (const std::exception& e) {
    std::cerr << "Whisper API processing error: " << e.what() << std::endl;
    result.success = false;
  }
  
  auto end_time = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  result.processing_time_ms = duration.count();
  
  return result;
}

bool WhisperAPIEngine::StartRealTimeRecognition(STTCallback callback) {
  if (real_time_active_) {
    return false;
  }
  
  real_time_callback_ = callback;
  real_time_active_ = true;
  
  real_time_thread_ = std::thread([this]() {
    while (real_time_active_) {
      // Record audio chunks
      AudioInput audio = RecordAudio(3.0f);
      
      if (!audio.samples.empty()) {
        STTResult result = ProcessAudio(audio);
        if (result.success && !result.text.empty() && real_time_callback_) {
          real_time_callback_(result);
        }
      }
      
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  
  return true;
}

void WhisperAPIEngine::StopRealTimeRecognition() {
  real_time_active_ = false;
  if (real_time_thread_.joinable()) {
    real_time_thread_.join();
  }
}

void WhisperAPIEngine::SetLanguage(const std::string& language) {
  language_ = language;
}

std::vector<std::string> WhisperAPIEngine::GetSupportedLanguages() {
  return {
    "auto", "en", "es", "fr", "de", "it", "pt", "ru", "ja", "ko", 
    "zh", "ar", "hi", "tr", "pl", "nl", "sv", "da", "no", "fi",
    "cs", "sk", "bg", "hr", "sl", "et", "lv", "lt", "hu", "ro"
  };
}

void WhisperAPIEngine::SetVAD(bool enable) {
  vad_enabled_ = enable;
}

void WhisperAPIEngine::Cleanup() {
  StopRealTimeRecognition();
  initialized_ = false;
}

std::string WhisperAPIEngine::SaveAudioToFile(const AudioInput& audio, const std::string& format) {
  // Create temporary WAV file first
  std::string temp_wav = "/tmp/whisper_api_" + std::to_string(std::rand()) + ".wav";
  
  std::ofstream file(temp_wav, std::ios::binary);
  if (!file.is_open()) {
    return "";
  }
  
  // Write WAV header
  const int data_size = audio.samples.size() * sizeof(int16_t);
  const int file_size = 36 + data_size;
  const int byte_rate = audio.sample_rate * audio.channels * 2;
  
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&file_size), 4);
  file.write("WAVE", 4);
  file.write("fmt ", 4);
  const int fmt_size = 16;
  file.write(reinterpret_cast<const char*>(&fmt_size), 4);
  const short audio_format = 1;
  file.write(reinterpret_cast<const char*>(&audio_format), 2);
  const short channels = audio.channels;
  file.write(reinterpret_cast<const char*>(&channels), 2);
  file.write(reinterpret_cast<const char*>(&audio.sample_rate), 4);
  file.write(reinterpret_cast<const char*>(&byte_rate), 4);
  const short block_align = audio.channels * 2;
  file.write(reinterpret_cast<const char*>(&block_align), 2);
  const short bits_per_sample = 16;
  file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&data_size), 4);
  file.write(reinterpret_cast<const char*>(audio.samples.data()), data_size);
  file.close();
  
  // Convert to MP3 if requested
  if (format == "mp3") {
    std::string temp_mp3 = "/tmp/whisper_api_" + std::to_string(std::rand()) + ".mp3";
    
    std::ostringstream cmd;
    cmd << "ffmpeg -y -loglevel quiet -i \"" << temp_wav << "\" -acodec libmp3lame -b:a 64k \"" << temp_mp3 << "\" 2>/dev/null";
    
    if (std::system(cmd.str().c_str()) == 0 && file_exists(temp_mp3)) {
      remove_file(temp_wav);
      return temp_mp3;
    } else {
      remove_file(temp_wav);
      return "";
    }
  }
  
  return temp_wav;
}

std::string WhisperAPIEngine::MakeAPIRequest(const std::string& audio_file) {
  // Create temporary file for response
  std::string response_file = "/tmp/whisper_response_" + std::to_string(std::rand()) + ".json";
  
  // Build curl command for OpenAI Whisper API
  std::ostringstream cmd;
  cmd << "curl -s -X POST https://api.openai.com/v1/audio/transcriptions"
      << " -H \"Authorization: Bearer " << api_key_ << "\""
      << " -H \"Content-Type: multipart/form-data\""
      << " -F \"file=@" << audio_file << "\""
      << " -F \"model=" << model_ << "\"";
  
  if (language_ != "auto") {
    cmd << " -F \"language=" << language_ << "\"";
  }
  
  cmd << " -F \"response_format=json\""
      << " -o \"" << response_file << "\" 2>/dev/null";
  
  // Execute the request
  int result = std::system(cmd.str().c_str());
  
  if (result == 0 && file_exists(response_file)) {
    // Read the response
    std::ifstream file(response_file);
    std::string response;
    if (file.is_open()) {
      std::string line;
      while (std::getline(file, line)) {
        response += line;
      }
      file.close();
    }
    
    remove_file(response_file);
    return response;
  }
  
  if (file_exists(response_file)) {
    remove_file(response_file);
  }
  
  return "";
}

std::string WhisperAPIEngine::ParseAPIResponse(const std::string& response) {
  // Simple JSON parsing for the "text" field
  // In a production system, you'd use a proper JSON library
  
  size_t text_pos = response.find("\"text\":");
  if (text_pos == std::string::npos) {
    return "";
  }
  
  // Find the opening quote
  size_t quote_start = response.find("\"", text_pos + 7);
  if (quote_start == std::string::npos) {
    return "";
  }
  
  // Find the closing quote (handle escaped quotes)
  size_t quote_end = quote_start + 1;
  while (quote_end < response.length()) {
    if (response[quote_end] == '"' && (quote_end == 0 || response[quote_end - 1] != '\\')) {
      break;
    }
    quote_end++;
  }
  
  if (quote_end >= response.length()) {
    return "";
  }
  
  return response.substr(quote_start + 1, quote_end - quote_start - 1);
}

AudioInput WhisperAPIEngine::RecordAudio(float duration_seconds) {
  AudioInput audio;
  
  // Create temporary file for recording
  std::string temp_file = "/tmp/record_api_" + std::to_string(std::rand()) + ".wav";
  
  // Record audio using ffmpeg
  std::ostringstream cmd;
  cmd << "ffmpeg -y -loglevel quiet -f avfoundation -i \":0\" -t " << duration_seconds 
      << " -ar 16000 -ac 1 -sample_fmt s16 \"" << temp_file << "\" 2>/dev/null";
  
  if (std::system(cmd.str().c_str()) == 0 && file_exists(temp_file)) {
    // Read the recorded file
    std::ifstream file(temp_file, std::ios::binary);
    if (file.is_open()) {
      // Skip WAV header
      file.seekg(44);
      
      // Read audio data
      int16_t sample;
      while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
        audio.samples.push_back(sample);
      }
      
      audio.sample_rate = 16000;
      audio.channels = 1;
      audio.duration_seconds = duration_seconds;
      
      file.close();
    }
    
    remove_file(temp_file);
  }
  
  return audio;
}

}  // namespace hobot_tts

bool MacOSDictationEngine::IsDictationEnabled() {
  // Simplified check - in real implementation, would check system preferences
  return std::system("which say > /dev/null 2>&1") == 0; // Basic macOS check
}

}  // namespace hobot_tts
