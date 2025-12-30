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

#include "audio_manager.h"
#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstring>

// C++14 compatible file operations
namespace {
  bool remove_file(const std::string& filename) {
    return std::remove(filename.c_str()) == 0;
  }
}

namespace hobot_tts {

// AudioManager factory method
std::unique_ptr<AudioManager> AudioManager::Create(const std::string& device_name) {
#ifdef __APPLE__
  // Try CoreAudio first, fall back to simple implementation
  auto core_audio = std::make_unique<CoreAudioManager>(device_name);
  if (core_audio->Initialize()) {
    return std::move(core_audio);
  }
#endif
  
  // Fall back to simple implementation using system commands
  auto simple_audio = std::make_unique<SimpleAudioManager>(device_name);
  if (simple_audio->Initialize()) {
    return std::move(simple_audio);
  }
  
  return nullptr;
}

#ifdef __APPLE__
// CoreAudioManager implementation
CoreAudioManager::CoreAudioManager(const std::string& device_name) 
  : device_name_(device_name), audio_unit_(nullptr), initialized_(false) {
}

CoreAudioManager::~CoreAudioManager() {
  Cleanup();
}

bool CoreAudioManager::Initialize() {
  if (!SetupAudioUnit()) {
    std::cerr << "Error: Failed to setup AudioUnit" << std::endl;
    return false;
  }
  
  initialized_ = true;
  return true;
}

bool CoreAudioManager::PlayAudio(const std::vector<int16_t>& audio_data, int sample_rate, int channels) {
  if (!initialized_ || audio_data.empty()) {
    return false;
  }
  
  // Setup callback data
  callback_data_.audio_data = audio_data;
  callback_data_.current_frame = 0;
  callback_data_.sample_rate = sample_rate;
  callback_data_.channels = channels;
  callback_data_.finished = false;
  
  // Start playback
  OSStatus status = AudioOutputUnitStart(audio_unit_);
  if (status != noErr) {
    std::cerr << "Error: Failed to start AudioUnit: " << status << std::endl;
    return false;
  }
  
  // Wait for playback to finish
  while (!callback_data_.finished) {
    usleep(10000); // Sleep for 10ms
  }
  
  // Stop playback
  AudioOutputUnitStop(audio_unit_);
  
  return true;
}

std::vector<AudioDevice> CoreAudioManager::GetAvailableDevices() {
  std::vector<AudioDevice> devices;
  
  UInt32 property_size = 0;
  OSStatus status = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices,
                                                &property_size, nullptr);
  if (status != noErr) {
    return devices;
  }
  
  UInt32 device_count = property_size / sizeof(AudioDeviceID);
  std::vector<AudioDeviceID> device_ids(device_count);
  
  status = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
                                   &property_size, device_ids.data());
  if (status != noErr) {
    return devices;
  }
  
  for (const auto& device_id : device_ids) {
    AudioDevice device;
    
    // Get device name
    property_size = sizeof(CFStringRef);
    CFStringRef device_name;
    status = AudioDeviceGetProperty(device_id, 0, false,
                                   kAudioDevicePropertyDeviceNameCFString,
                                   &property_size, &device_name);
    if (status == noErr) {
      char name_buffer[256];
      CFStringGetCString(device_name, name_buffer, sizeof(name_buffer), kCFStringEncodingUTF8);
      device.name = name_buffer;
      device.id = std::to_string(device_id);
      CFRelease(device_name);
    }
    
    // Get channel count
    property_size = 0;
    status = AudioDeviceGetPropertyInfo(device_id, 0, false,
                                       kAudioDevicePropertyStreamConfiguration,
                                       &property_size, nullptr);
    if (status == noErr && property_size > 0) {
      std::vector<char> buffer(property_size);
      AudioBufferList* buffer_list = reinterpret_cast<AudioBufferList*>(buffer.data());
      
      status = AudioDeviceGetProperty(device_id, 0, false,
                                     kAudioDevicePropertyStreamConfiguration,
                                     &property_size, buffer_list);
      if (status == noErr) {
        device.channels = buffer_list->mNumberBuffers;
      }
    }
    
    devices.push_back(device);
  }
  
  return devices;
}

bool CoreAudioManager::SetOutputDevice(const std::string& device_name) {
  device_name_ = device_name;
  return true; // Simplified implementation
}

void CoreAudioManager::Cleanup() {
  if (audio_unit_) {
    TeardownAudioUnit();
  }
  initialized_ = false;
}

OSStatus CoreAudioManager::AudioCallback(void* inRefCon,
                                        AudioUnitRenderActionFlags* ioActionFlags,
                                        const AudioTimeStamp* inTimeStamp,
                                        UInt32 inBusNumber,
                                        UInt32 inNumberFrames,
                                        AudioBufferList* ioData) {
  CallbackData* data = static_cast<CallbackData*>(inRefCon);
  
  if (!data || data->finished) {
    return noErr;
  }
  
  AudioBuffer& buffer = ioData->mBuffers[0];
  int16_t* output = static_cast<int16_t*>(buffer.mData);
  
  UInt32 frames_to_copy = std::min(inNumberFrames, 
                                   static_cast<UInt32>(data->audio_data.size() - data->current_frame));
  
  if (frames_to_copy > 0) {
    std::memcpy(output, &data->audio_data[data->current_frame], frames_to_copy * sizeof(int16_t));
    data->current_frame += frames_to_copy;
  }
  
  // Fill remaining frames with silence
  if (frames_to_copy < inNumberFrames) {
    std::memset(&output[frames_to_copy], 0, (inNumberFrames - frames_to_copy) * sizeof(int16_t));
    data->finished = true;
  }
  
  return noErr;
}

bool CoreAudioManager::SetupAudioUnit() {
  AudioComponentDescription desc = {};
  desc.componentType = kAudioUnitType_Output;
  desc.componentSubType = kAudioUnitSubType_DefaultOutput;
  desc.componentManufacturer = kAudioUnitManufacturer_Apple;
  
  AudioComponent component = AudioComponentFindNext(nullptr, &desc);
  if (!component) {
    return false;
  }
  
  OSStatus status = AudioComponentInstanceNew(component, &audio_unit_);
  if (status != noErr) {
    return false;
  }
  
  // Setup audio format
  AudioStreamBasicDescription format = {};
  format.mSampleRate = 16000.0;
  format.mFormatID = kAudioFormatLinearPCM;
  format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
  format.mFramesPerPacket = 1;
  format.mChannelsPerFrame = 1;
  format.mBitsPerChannel = 16;
  format.mBytesPerFrame = 2;
  format.mBytesPerPacket = 2;
  
  status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_StreamFormat,
                               kAudioUnitScope_Input, 0, &format, sizeof(format));
  if (status != noErr) {
    return false;
  }
  
  // Set callback
  AURenderCallbackStruct callback_struct = {};
  callback_struct.inputProc = AudioCallback;
  callback_struct.inputProcRefCon = &callback_data_;
  
  status = AudioUnitSetProperty(audio_unit_, kAudioUnitProperty_SetRenderCallback,
                               kAudioUnitScope_Input, 0, &callback_struct, sizeof(callback_struct));
  if (status != noErr) {
    return false;
  }
  
  // Initialize the unit
  status = AudioUnitInitialize(audio_unit_);
  return status == noErr;
}

void CoreAudioManager::TeardownAudioUnit() {
  if (audio_unit_) {
    AudioOutputUnitStop(audio_unit_);
    AudioUnitUninitialize(audio_unit_);
    AudioComponentInstanceDispose(audio_unit_);
    audio_unit_ = nullptr;
  }
}

AudioDeviceID CoreAudioManager::GetDeviceID(const std::string& device_name) {
  // Simplified implementation - return default device
  UInt32 property_size = sizeof(AudioDeviceID);
  AudioDeviceID device_id;
  OSStatus status = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
                                            &property_size, &device_id);
  return (status == noErr) ? device_id : 0;
}
#endif

// SimpleAudioManager implementation
SimpleAudioManager::SimpleAudioManager(const std::string& device_name) 
  : device_name_(device_name), initialized_(false) {
}

SimpleAudioManager::~SimpleAudioManager() {
  Cleanup();
}

bool SimpleAudioManager::Initialize() {
  // Check if afplay is available (macOS)
  if (std::system("which afplay > /dev/null 2>&1") == 0) {
    initialized_ = true;
    return true;
  }
  
  // Check if aplay is available (Linux)
  if (std::system("which aplay > /dev/null 2>&1") == 0) {
    initialized_ = true;
    return true;
  }
  
  std::cerr << "Error: No suitable audio player found (afplay or aplay)" << std::endl;
  return false;
}

bool SimpleAudioManager::PlayAudio(const std::vector<int16_t>& audio_data, int sample_rate, int channels) {
  if (!initialized_ || audio_data.empty()) {
    return false;
  }
  
  return PlayWithSystemCommand(audio_data, sample_rate, channels);
}

std::vector<AudioDevice> SimpleAudioManager::GetAvailableDevices() {
  std::vector<AudioDevice> devices;
  AudioDevice default_device;
  default_device.name = "Default";
  default_device.id = "default";
  default_device.channels = 2;
  default_device.sample_rate = 44100;
  devices.push_back(default_device);
  return devices;
}

bool SimpleAudioManager::SetOutputDevice(const std::string& device_name) {
  device_name_ = device_name;
  return true;
}

void SimpleAudioManager::Cleanup() {
  initialized_ = false;
}

bool SimpleAudioManager::PlayWithSystemCommand(const std::vector<int16_t>& audio_data, int sample_rate, int channels) {
  // Create temporary WAV file
  std::string temp_file = CreateTempWavFile(audio_data, sample_rate, channels);
  if (temp_file.empty()) {
    return false;
  }
  
  // Build command based on available player
  std::string cmd;
  if (std::system("which afplay > /dev/null 2>&1") == 0) {
    cmd = "afplay \"" + temp_file + "\"";
  } else if (std::system("which aplay > /dev/null 2>&1") == 0) {
    cmd = "aplay \"" + temp_file + "\"";
  } else {
    remove_file(temp_file);
    return false;
  }
  
  // Play the file
  int result = std::system(cmd.c_str());
  
  // Clean up
  remove_file(temp_file);
  
  return result == 0;
}

std::string SimpleAudioManager::CreateTempWavFile(const std::vector<int16_t>& audio_data, int sample_rate, int channels) {
  std::string temp_file = "/tmp/hobot_tts_audio_" + std::to_string(std::rand()) + ".wav";
  
  std::ofstream file(temp_file, std::ios::binary);
  if (!file.is_open()) {
    return "";
  }
  
  // WAV header
  const int data_size = audio_data.size() * sizeof(int16_t);
  const int file_size = 36 + data_size;
  const int byte_rate = sample_rate * channels * 2; // 16-bit
  
  // RIFF header
  file.write("RIFF", 4);
  file.write(reinterpret_cast<const char*>(&file_size), 4);
  file.write("WAVE", 4);
  
  // Format chunk
  file.write("fmt ", 4);
  const int fmt_size = 16;
  file.write(reinterpret_cast<const char*>(&fmt_size), 4);
  const short audio_format = 1; // PCM
  file.write(reinterpret_cast<const char*>(&audio_format), 2);
  file.write(reinterpret_cast<const char*>(&channels), 2);
  file.write(reinterpret_cast<const char*>(&sample_rate), 4);
  file.write(reinterpret_cast<const char*>(&byte_rate), 4);
  const short block_align = channels * 2;
  file.write(reinterpret_cast<const char*>(&block_align), 2);
  const short bits_per_sample = 16;
  file.write(reinterpret_cast<const char*>(&bits_per_sample), 2);
  
  // Data chunk
  file.write("data", 4);
  file.write(reinterpret_cast<const char*>(&data_size), 4);
  file.write(reinterpret_cast<const char*>(audio_data.data()), data_size);
  
  file.close();
  
  return temp_file;
}

}  // namespace hobot_tts
