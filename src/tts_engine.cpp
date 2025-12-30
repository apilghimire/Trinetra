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

#include "tts_engine.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdio>

// C++14 compatible file operations
namespace {
  bool remove_file(const std::string& filename) {
    return std::remove(filename.c_str()) == 0;
  }
}

namespace hobot_tts {

// TTSEngine factory method
std::unique_ptr<TTSEngine> TTSEngine::Create(TTSEngineType type, const std::string& voice) {
  switch (type) {
    case TTSEngineType::SAY:
      return std::make_unique<SayEngine>(voice);
    case TTSEngineType::FESTIVAL:
      return std::make_unique<FestivalEngine>(voice);
    case TTSEngineType::ESPEAK:
      return std::make_unique<ESpeakEngine>(voice);
    default:
      return std::make_unique<SayEngine>(voice);
  }
}

// SayEngine implementation
SayEngine::SayEngine(const std::string& voice) 
  : voice_name_(voice.empty() ? "default" : voice), initialized_(false) {
}

SayEngine::~SayEngine() {
  Cleanup();
}

bool SayEngine::Initialize() {
  // Check if 'say' command is available
  if (std::system("which say > /dev/null 2>&1") != 0) {
    std::cerr << "Error: 'say' command not found. This engine requires macOS." << std::endl;
    return false;
  }
  
  initialized_ = true;
  return true;
}

bool SayEngine::Synthesize(const std::string& text, AudioData& audio_data) {
  if (!initialized_) {
    std::cerr << "Error: SayEngine not initialized" << std::endl;
    return false;
  }
  
  if (text.empty()) {
    return false;
  }
  
  // Create temporary file for audio output
  std::string temp_file = "/tmp/hobot_tts_" + std::to_string(std::rand()) + ".aiff";
  
  // Build say command
  std::ostringstream cmd;
  cmd << "say";
  
  if (voice_name_ != "default") {
    cmd << " -v \"" << voice_name_ << "\"";
  }
  
  cmd << " -o \"" << temp_file << "\"";
  cmd << " \"" << EscapeText(text) << "\"";
  
  // Execute the command
  int result = std::system(cmd.str().c_str());
  if (result != 0) {
    std::cerr << "Error: say command failed with code " << result << std::endl;
    std::cerr << "Command: " << cmd.str() << std::endl;
    return false;
  }
  
  // Convert audio file to PCM data
  bool success = ConvertAudioFile(temp_file, audio_data);
  
  // Clean up temporary file
  remove_file(temp_file);
  
  return success;
}

void SayEngine::SetVoice(const std::string& voice_name) {
  voice_name_ = voice_name.empty() ? "default" : voice_name;
}

std::vector<std::string> SayEngine::GetAvailableVoices() {
  std::vector<std::string> voices;
  
  if (!initialized_) {
    return voices;
  }
  
  // Get list of available voices
  FILE* pipe = popen("say -v ? 2>/dev/null", "r");
  if (!pipe) {
    return voices;
  }
  
  char buffer[256];
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    std::string line(buffer);
    // Extract voice name (first word on each line)
    size_t space_pos = line.find(' ');
    if (space_pos != std::string::npos) {
      voices.push_back(line.substr(0, space_pos));
    }
  }
  
  pclose(pipe);
  return voices;
}

void SayEngine::Cleanup() {
  initialized_ = false;
}

std::string SayEngine::EscapeText(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.length() * 2);
  
  for (char c : text) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') {
      escaped += '\\';
    }
    escaped += c;
  }
  
  return escaped;
}

bool SayEngine::ConvertAudioFile(const std::string& input_file, AudioData& audio_data) {
  // Convert AIFF to WAV using afconvert
  std::string wav_file = input_file + ".wav";
  
  std::ostringstream cmd;
  cmd << "afconvert -f WAVE -d LEI16@16000 \"" << input_file << "\" \"" << wav_file << "\"";
  
  if (std::system(cmd.str().c_str()) != 0) {
    std::cerr << "Error: Failed to convert audio file" << std::endl;
    return false;
  }
  
  // Read WAV file
  std::ifstream file(wav_file, std::ios::binary);
  if (!file.is_open()) {
    remove_file(wav_file);
    return false;
  }
  
  // Skip WAV header (44 bytes)
  file.seekg(44);
  
  // Read audio data
  audio_data.samples.clear();
  int16_t sample;
  while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
    audio_data.samples.push_back(sample);
  }
  
  file.close();
  remove_file(wav_file);
  
  audio_data.sample_rate = 16000;
  audio_data.channels = 1;
  
  return !audio_data.samples.empty();
}

// FestivalEngine implementation
FestivalEngine::FestivalEngine(const std::string& voice) 
  : voice_name_(voice.empty() ? "default" : voice), initialized_(false) {
}

FestivalEngine::~FestivalEngine() {
  Cleanup();
}

bool FestivalEngine::Initialize() {
  // Check if festival is available
  if (std::system("which festival > /dev/null 2>&1") != 0) {
    std::cerr << "Error: 'festival' command not found. Please install Festival TTS." << std::endl;
    return false;
  }
  
  initialized_ = true;
  return true;
}

bool FestivalEngine::Synthesize(const std::string& text, AudioData& audio_data) {
  if (!initialized_) {
    std::cerr << "Error: FestivalEngine not initialized" << std::endl;
    return false;
  }
  
  if (text.empty()) {
    return false;
  }
  
  // Create temporary files
  std::string temp_text = "/tmp/hobot_tts_" + std::to_string(std::rand()) + ".txt";
  std::string temp_audio = "/tmp/hobot_tts_" + std::to_string(std::rand()) + ".wav";
  
  // Write text to file
  std::ofstream text_file(temp_text);
  if (!text_file.is_open()) {
    return false;
  }
  text_file << text;
  text_file.close();
  
  // Build festival command
  std::ostringstream cmd;
  cmd << "festival --tts \"" << temp_text << "\" --otype wav --output \"" << temp_audio << "\"";
  
  // Execute the command
  int result = std::system(cmd.str().c_str());
  
  // Clean up text file
  remove_file(temp_text);
  
  if (result != 0) {
    std::cerr << "Error: festival command failed" << std::endl;
    return false;
  }
  
  // Convert audio file to PCM data
  bool success = ConvertAudioFile(temp_audio, audio_data);
  
  // Clean up audio file
  remove_file(temp_audio);
  
  return success;
}

void FestivalEngine::SetVoice(const std::string& voice_name) {
  voice_name_ = voice_name.empty() ? "default" : voice_name;
}

std::vector<std::string> FestivalEngine::GetAvailableVoices() {
  std::vector<std::string> voices;
  voices.push_back("default");
  // Festival voice discovery could be implemented here
  return voices;
}

void FestivalEngine::Cleanup() {
  initialized_ = false;
}

std::string FestivalEngine::EscapeText(const std::string& text) {
  return text; // Festival handles text escaping internally
}

bool FestivalEngine::ConvertAudioFile(const std::string& input_file, AudioData& audio_data) {
  // Read WAV file directly (Festival outputs WAV)
  std::ifstream file(input_file, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  
  // Skip WAV header (44 bytes)
  file.seekg(44);
  
  // Read audio data
  audio_data.samples.clear();
  int16_t sample;
  while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
    audio_data.samples.push_back(sample);
  }
  
  file.close();
  
  audio_data.sample_rate = 16000;
  audio_data.channels = 1;
  
  return !audio_data.samples.empty();
}

// ESpeakEngine implementation
ESpeakEngine::ESpeakEngine(const std::string& voice) 
  : voice_name_(voice.empty() ? "en" : voice), initialized_(false) {
}

ESpeakEngine::~ESpeakEngine() {
  Cleanup();
}

bool ESpeakEngine::Initialize() {
  // Check if espeak is available
  if (std::system("which espeak > /dev/null 2>&1") != 0) {
    std::cerr << "Error: 'espeak' command not found. Please install eSpeak." << std::endl;
    return false;
  }
  
  initialized_ = true;
  return true;
}

bool ESpeakEngine::Synthesize(const std::string& text, AudioData& audio_data) {
  if (!initialized_) {
    std::cerr << "Error: ESpeakEngine not initialized" << std::endl;
    return false;
  }
  
  if (text.empty()) {
    return false;
  }
  
  // Create temporary file for audio output
  std::string temp_file = "/tmp/hobot_tts_" + std::to_string(std::rand()) + ".wav";
  
  // Build espeak command
  std::ostringstream cmd;
  cmd << "espeak -v " << voice_name_ << " -s 150 -w \"" << temp_file << "\" \"" << EscapeText(text) << "\"";
  
  // Execute the command
  int result = std::system(cmd.str().c_str());
  if (result != 0) {
    std::cerr << "Error: espeak command failed" << std::endl;
    return false;
  }
  
  // Convert audio file to PCM data
  bool success = ConvertAudioFile(temp_file, audio_data);
  
  // Clean up temporary file
  remove_file(temp_file);
  
  return success;
}

void ESpeakEngine::SetVoice(const std::string& voice_name) {
  voice_name_ = voice_name.empty() ? "en" : voice_name;
}

std::vector<std::string> ESpeakEngine::GetAvailableVoices() {
  std::vector<std::string> voices;
  
  if (!initialized_) {
    return voices;
  }
  
  // Get list of available voices
  FILE* pipe = popen("espeak --voices 2>/dev/null", "r");
  if (!pipe) {
    return voices;
  }
  
  char buffer[256];
  bool first_line = true;
  while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    if (first_line) {
      first_line = false;
      continue; // Skip header line
    }
    
    std::string line(buffer);
    std::istringstream iss(line);
    std::string age, gender, voice_name;
    iss >> age >> voice_name >> gender;
    
    if (!voice_name.empty()) {
      voices.push_back(voice_name);
    }
  }
  
  pclose(pipe);
  return voices;
}

void ESpeakEngine::Cleanup() {
  initialized_ = false;
}

std::string ESpeakEngine::EscapeText(const std::string& text) {
  std::string escaped;
  escaped.reserve(text.length() * 2);
  
  for (char c : text) {
    if (c == '"' || c == '\\' || c == '$' || c == '`') {
      escaped += '\\';
    }
    escaped += c;
  }
  
  return escaped;
}

bool ESpeakEngine::ConvertAudioFile(const std::string& input_file, AudioData& audio_data) {
  // eSpeak outputs WAV directly
  std::ifstream file(input_file, std::ios::binary);
  if (!file.is_open()) {
    return false;
  }
  
  // Skip WAV header (44 bytes)
  file.seekg(44);
  
  // Read audio data
  audio_data.samples.clear();
  int16_t sample;
  while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
    audio_data.samples.push_back(sample);
  }
  
  file.close();
  
  audio_data.sample_rate = 16000;
  audio_data.channels = 1;
  
  return !audio_data.samples.empty();
}

}  // namespace hobot_tts
