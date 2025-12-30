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

#include "microphone_recorder.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <algorithm>

#ifdef __APPLE__
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace hobot_tts {

// C++14 compatible utility functions
namespace {
  bool file_exists(const std::string& filename) {
    std::ifstream file(filename);
    return file.good();
  }
  
  void remove_file(const std::string& filename) {
    std::remove(filename.c_str());
  }
}

MicrophoneRecorder::MicrophoneRecorder()
  : sample_rate_(16000), channels_(1), bits_per_sample_(16),
    recording_(false), vad_enabled_(true), silence_threshold_(0.01f),
    min_speech_duration_(0.5f), max_silence_duration_(1.0f),
    buffer_duration_(10.0f) {
}

MicrophoneRecorder::~MicrophoneRecorder() {
  StopRecording();
}

bool MicrophoneRecorder::Initialize() {
  // Check if we have recording capabilities
  if (!CheckRecordingCapability()) {
    std::cerr << "No recording capability detected" << std::endl;
    return false;
  }
  
  initialized_ = true;
  std::cout << "✓ Microphone recorder initialized (16kHz, mono)" << std::endl;
  return true;
}

void MicrophoneRecorder::SetRecordingParameters(int sample_rate, int channels, int bits_per_sample) {
  if (recording_) {
    std::cerr << "Cannot change parameters while recording" << std::endl;
    return;
  }
  
  sample_rate_ = sample_rate;
  channels_ = channels;
  bits_per_sample_ = bits_per_sample;
}

void MicrophoneRecorder::SetVADParameters(bool enable, float silence_threshold, 
                                        float min_speech_duration, float max_silence_duration) {
  vad_enabled_ = enable;
  silence_threshold_ = silence_threshold;
  min_speech_duration_ = min_speech_duration;
  max_silence_duration_ = max_silence_duration;
}

AudioInput MicrophoneRecorder::RecordAudio(float duration) {
  AudioInput audio;
  
  if (!initialized_) {
    std::cerr << "Recorder not initialized" << std::endl;
    return audio;
  }
  
  std::cout << "🎤 Recording " << duration << " seconds..." << std::endl;
  
  // Create temporary file for recording
  std::string temp_file = "/tmp/mic_record_" + std::to_string(std::rand()) + ".wav";
  
  // Choose recording method based on available tools
  std::string cmd;
  if (std::system("which sox > /dev/null 2>&1") == 0) {
    // Use SoX if available
    cmd = "rec -q -t wav -c " + std::to_string(channels_) + 
          " -r " + std::to_string(sample_rate_) + 
          " -b " + std::to_string(bits_per_sample_) + 
          " \"" + temp_file + "\" trim 0 " + std::to_string(duration);
  } else if (std::system("which ffmpeg > /dev/null 2>&1") == 0) {
    // Use FFmpeg as fallback
    cmd = "ffmpeg -y -loglevel quiet -f avfoundation -i \":0\" -t " + 
          std::to_string(duration) + 
          " -ar " + std::to_string(sample_rate_) + 
          " -ac " + std::to_string(channels_) + 
          " -sample_fmt s16 \"" + temp_file + "\" 2>/dev/null";
  } else {
    std::cerr << "No recording tool available (install sox or ffmpeg)" << std::endl;
    return audio;
  }
  
  auto start_time = std::chrono::high_resolution_clock::now();
  
  if (std::system(cmd.c_str()) == 0 && file_exists(temp_file)) {
    audio = ReadWAVFile(temp_file);
    audio.duration_seconds = duration;
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto actual_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    std::cout << "✓ Recorded " << audio.samples.size() << " samples in " 
              << actual_duration.count() << "ms" << std::endl;
  } else {
    std::cerr << "Recording failed" << std::endl;
  }
  
  remove_file(temp_file);
  return audio;
}

AudioInput MicrophoneRecorder::RecordWithVAD() {
  AudioInput audio;
  
  if (!initialized_ || !vad_enabled_) {
    return RecordAudio(5.0f); // Default 5 second recording
  }
  
  std::cout << "🎤 Recording with VAD (speak when ready)..." << std::endl;
  
  std::vector<int16_t> accumulated_samples;
  bool speech_detected = false;
  auto speech_start_time = std::chrono::high_resolution_clock::now();
  auto last_speech_time = speech_start_time;
  
  const float chunk_duration = 0.1f; // 100ms chunks
  const size_t max_chunks = static_cast<size_t>(buffer_duration_ / chunk_duration);
  
  for (size_t chunk = 0; chunk < max_chunks; ++chunk) {
    AudioInput chunk_audio = RecordAudio(chunk_duration);
    
    if (chunk_audio.samples.empty()) {
      break;
    }
    
    // Calculate RMS energy for VAD
    float energy = CalculateRMS(chunk_audio.samples);
    bool has_speech = energy > silence_threshold_;
    
    if (has_speech) {
      if (!speech_detected) {
        std::cout << "🗣️  Speech detected, recording..." << std::endl;
        speech_detected = true;
        speech_start_time = std::chrono::high_resolution_clock::now();
      }
      last_speech_time = std::chrono::high_resolution_clock::now();
      
      // Add chunk to accumulated samples
      accumulated_samples.insert(accumulated_samples.end(), 
                                chunk_audio.samples.begin(), 
                                chunk_audio.samples.end());
    } else if (speech_detected) {
      // Check if we've had enough silence
      auto now = std::chrono::high_resolution_clock::now();
      auto silence_duration = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_speech_time).count();
      
      if (silence_duration >= max_silence_duration_) {
        auto total_speech_duration = std::chrono::duration_cast<std::chrono::duration<float>>(last_speech_time - speech_start_time).count();
        
        if (total_speech_duration >= min_speech_duration_) {
          std::cout << "✓ Recording complete (" << total_speech_duration << "s speech)" << std::endl;
          break;
        } else {
          std::cout << "⚠️  Speech too short, continuing..." << std::endl;
          speech_detected = false;
          accumulated_samples.clear();
        }
      } else {
        // Add silence chunk too (for natural speech gaps)
        accumulated_samples.insert(accumulated_samples.end(), 
                                  chunk_audio.samples.begin(), 
                                  chunk_audio.samples.end());
      }
    }
    
    // Visual feedback
    if (chunk % 10 == 0) { // Every second
      std::cout << (has_speech ? "🔊" : "🔇") << std::flush;
    }
  }
  
  std::cout << std::endl;
  
  if (!accumulated_samples.empty()) {
    audio.samples = accumulated_samples;
    audio.sample_rate = sample_rate_;
    audio.channels = channels_;
    audio.duration_seconds = static_cast<float>(accumulated_samples.size()) / sample_rate_;
    
    std::cout << "✓ VAD recording complete: " << audio.duration_seconds << "s, " 
              << accumulated_samples.size() << " samples" << std::endl;
  } else {
    std::cout << "❌ No speech detected" << std::endl;
  }
  
  return audio;
}

bool MicrophoneRecorder::StartContinuousRecording(RecordingCallback callback) {
  if (recording_ || !initialized_) {
    return false;
  }
  
  recording_ = true;
  recording_callback_ = callback;
  
  recording_thread_ = std::thread([this]() {
    std::cout << "🎤 Starting continuous recording..." << std::endl;
    
    while (recording_) {
      AudioInput audio;
      
      if (vad_enabled_) {
        audio = RecordWithVAD();
      } else {
        audio = RecordAudio(3.0f); // 3-second chunks
      }
      
      if (!audio.samples.empty() && recording_callback_) {
        recording_callback_(audio);
      }
      
      // Small delay between recordings
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  });
  
  return true;
}

void MicrophoneRecorder::StopContinuousRecording() {
  recording_ = false;
  if (recording_thread_.joinable()) {
    recording_thread_.join();
  }
}

bool MicrophoneRecorder::StopRecording() {
  StopContinuousRecording();
  return true;
}

bool MicrophoneRecorder::IsRecording() const {
  return recording_;
}

RecordingInfo MicrophoneRecorder::GetRecordingInfo() const {
  RecordingInfo info;
  info.sample_rate = sample_rate_;
  info.channels = channels_;
  info.bits_per_sample = bits_per_sample_;
  info.vad_enabled = vad_enabled_;
  info.silence_threshold = silence_threshold_;
  info.min_speech_duration = min_speech_duration_;
  info.max_silence_duration = max_silence_duration_;
  info.is_recording = recording_;
  return info;
}

bool MicrophoneRecorder::CheckRecordingCapability() {
  // Check for recording tools
  if (std::system("which sox > /dev/null 2>&1") == 0) {
    std::cout << "✓ Found SoX for audio recording" << std::endl;
    return true;
  }
  
  if (std::system("which ffmpeg > /dev/null 2>&1") == 0) {
    std::cout << "✓ Found FFmpeg for audio recording" << std::endl;
    return true;
  }
  
  std::cout << "❌ No recording tools found. Install sox or ffmpeg:" << std::endl;
  std::cout << "   brew install sox" << std::endl;
  std::cout << "   brew install ffmpeg" << std::endl;
  return false;
}

AudioInput MicrophoneRecorder::ReadWAVFile(const std::string& filename) {
  AudioInput audio;
  
  std::ifstream file(filename, std::ios::binary);
  if (!file.is_open()) {
    return audio;
  }
  
  // Read WAV header (simplified)
  char header[44];
  file.read(header, 44);
  
  if (file.gcount() != 44) {
    return audio;
  }
  
  // Extract key information from header
  int32_t* sample_rate_ptr = reinterpret_cast<int32_t*>(&header[24]);
  int16_t* channels_ptr = reinterpret_cast<int16_t*>(&header[22]);
  int16_t* bits_per_sample_ptr = reinterpret_cast<int16_t*>(&header[34]);
  
  audio.sample_rate = *sample_rate_ptr;
  audio.channels = *channels_ptr;
  
  // Read audio data
  if (*bits_per_sample_ptr == 16) {
    int16_t sample;
    while (file.read(reinterpret_cast<char*>(&sample), sizeof(sample))) {
      audio.samples.push_back(sample);
    }
  } else {
    std::cerr << "Unsupported bit depth: " << *bits_per_sample_ptr << std::endl;
  }
  
  file.close();
  return audio;
}

float MicrophoneRecorder::CalculateRMS(const std::vector<int16_t>& samples) {
  if (samples.empty()) return 0.0f;
  
  double sum = 0.0;
  for (int16_t sample : samples) {
    double normalized = static_cast<double>(sample) / 32768.0;
    sum += normalized * normalized;
  }
  
  return std::sqrt(sum / samples.size());
}

}  // namespace hobot_tts
