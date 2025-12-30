// Test program for OpenAI Whisper API
// Compile with: clang++ -std=c++14 -o test_whisper_api test_whisper_api.cpp

#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <cstdint>
#include <chrono>
#include <sstream>

// Simplified audio input structure
struct SimpleAudioInput {
    std::vector<int16_t> samples;
    int sample_rate = 16000;
    int channels = 1;
    float duration_seconds = 0.0f;
};

// Simple STT result
struct SimpleSTTResult {
    std::string text;
    bool success = false;
    float confidence = 0.0f;
    long processing_time_ms = 0;
};

class SimpleWhisperAPI {
public:
    SimpleWhisperAPI(const std::string& api_key, const std::string& model = "whisper-1") 
        : api_key_(), model_(model) {}
    
    bool Initialize() {
        std::cout << "Initializing OpenAI Whisper API..." << std::endl;
        
        if (api_key_.empty()) {
            std::cerr << "❌ No API key provided" << std::endl;
            return false;
        }
        
        // Check if curl is available
        if (std::system("which curl > /dev/null 2>&1") != 0) {
            std::cerr << "❌ curl not found. Install with: brew install curl" << std::endl;
            return false;
        }
        
        // Check if ffmpeg is available
        if (std::system("which ffmpeg > /dev/null 2>&1") != 0) {
            std::cerr << "❌ ffmpeg not found. Install with: brew install ffmpeg" << std::endl;
            return false;
        }
        
        std::cout << "✓ OpenAI Whisper API initialized (model: " << model_ << ")" << std::endl;
        return true;
    }
    
    SimpleSTTResult ProcessAudio(const SimpleAudioInput& audio) {
        SimpleSTTResult result;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        if (audio.samples.empty()) {
            return result;
        }
        
        // Save audio to temporary file
        std::string audio_file = SaveAudioToMP3(audio);
        if (audio_file.empty()) {
            return result;
        }
        
        // Make API request
        std::string response = MakeAPIRequest(audio_file);
        
        // Clean up temporary file
        std::remove(audio_file.c_str());
        
        if (!response.empty()) {
            result.text = ParseResponse(response);
            result.success = !result.text.empty();
            result.confidence = 0.95f; // API doesn't provide confidence scores
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        result.processing_time_ms = duration.count();
        
        return result;
    }
    
private:
    std::string api_key_;
    std::string model_;
    
    std::string SaveAudioToMP3(const SimpleAudioInput& audio) {
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
        
        // Convert to MP3
        std::string temp_mp3 = "/tmp/whisper_api_" + std::to_string(std::rand()) + ".mp3";
        
        std::ostringstream cmd;
        cmd << "ffmpeg -y -loglevel quiet -i \"" << temp_wav << "\" -acodec libmp3lame -b:a 64k \"" << temp_mp3 << "\" 2>/dev/null";
        
        if (std::system(cmd.str().c_str()) == 0) {
            std::remove(temp_wav.c_str());
            return temp_mp3;
        } else {
            std::remove(temp_wav.c_str());
            return "";
        }
    }
    
    std::string MakeAPIRequest(const std::string& audio_file) {
        // Create temporary file for response
        std::string response_file = "/tmp/whisper_response_" + std::to_string(std::rand()) + ".json";
        
        // Build curl command for OpenAI Whisper API
        std::ostringstream cmd;
        cmd << "curl -s -X POST https://api.openai.com/v1/audio/transcriptions"
            << " -H \"Authorization: Bearer " << api_key_ << "\""
            << " -H \"Content-Type: multipart/form-data\""
            << " -F \"file=@" << audio_file << "\""
            << " -F \"model=" << model_ << "\""
            << " -F \"response_format=json\""
            << " -o \"" << response_file << "\" 2>/dev/null";
        
        // Execute the request
        int result = std::system(cmd.str().c_str());
        
        if (result == 0 && std::ifstream(response_file).good()) {
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
            
            std::remove(response_file.c_str());
            return response;
        }
        
        std::remove(response_file.c_str());
        return "";
    }
    
    std::string ParseResponse(const std::string& response) {
        // Simple JSON parsing for the "text" field
        size_t text_pos = response.find("\"text\":");
        if (text_pos == std::string::npos) {
            std::cerr << "❌ API Response: " << response << std::endl;
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
};

SimpleAudioInput RecordAudio(float duration) {
    SimpleAudioInput audio;
    
    std::cout << "🎤 Recording " << duration << " seconds... speak now!" << std::endl;
    
    std::string temp_file = "/tmp/record_test_" + std::to_string(std::rand()) + ".wav";
    
    // Record using ffmpeg
    std::ostringstream cmd;
    cmd << "ffmpeg -y -loglevel quiet -f avfoundation -i \":0\" -t " << duration 
        << " -ar 16000 -ac 1 -sample_fmt s16 \"" << temp_file << "\" 2>/dev/null";
    
    if (std::system(cmd.str().c_str()) == 0) {
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
            audio.duration_seconds = duration;
            
            file.close();
            std::cout << "✓ Recorded " << audio.samples.size() << " samples" << std::endl;
        }
        
        std::remove(temp_file.c_str());
    } else {
        std::cerr << "❌ Recording failed" << std::endl;
    }
    
    return audio;
}

std::string GetAPIKey() {
    // Try to get API key from environment variable
    const char* env_key = std::getenv("OPENAI_API_KEY");
    if (env_key) {
        return std::string(env_key);
    }
    
    // Ask user for API key
    std::cout << "Enter your OpenAI API key: ";
    std::string api_key;
    std::getline(std::cin, api_key);
    return api_key;
}

void TestAPISTT() {
    std::cout << "\n🤖 Testing OpenAI Whisper API..." << std::endl;
    
    std::string api_key = GetAPIKey();
    if (api_key.empty()) {
        std::cerr << "❌ No API key provided" << std::endl;
        return;
    }
    
    SimpleWhisperAPI stt(api_key);
    if (!stt.Initialize()) {
        std::cerr << "❌ Failed to initialize Whisper API" << std::endl;
        return;
    }
    
    std::cout << "\nWhen ready, press Enter and speak a test phrase..." << std::endl;
    std::cin.get();
    
    SimpleAudioInput audio = RecordAudio(5.0f);
    
    if (!audio.samples.empty()) {
        std::cout << "🤖 Processing speech with OpenAI API..." << std::endl;
        
        SimpleSTTResult result = stt.ProcessAudio(audio);
        
        if (result.success) {
            std::cout << "✅ API Recognition result: \"" << result.text << "\"" << std::endl;
            std::cout << "✅ Processing time: " << result.processing_time_ms << "ms" << std::endl;
            
            // Echo back with TTS
            std::cout << "🗣️  Speaking back: " << result.text << std::endl;
            std::string cmd = "say \"You said: " + result.text + "\"";
            std::system(cmd.c_str());
        } else {
            std::cout << "❌ API Recognition failed" << std::endl;
        }
    } else {
        std::cout << "❌ No audio recorded" << std::endl;
    }
}

void HandleCommand(const std::string& command);

void VoiceAssistantDemo() {
    std::cout << "\n🤖 Voice Assistant Demo with OpenAI API" << std::endl;
    std::cout << "Say 'Hello assistant' followed by a command" << std::endl;
    std::cout << "Commands: hello, time, weather, stop" << std::endl;
    std::cout << "Press Ctrl+C to exit\n" << std::endl;
    
    std::string api_key = GetAPIKey();
    if (api_key.empty()) {
        std::cerr << "❌ No API key provided" << std::endl;
        return;
    }
    
    SimpleWhisperAPI stt(api_key);
    if (!stt.Initialize()) {
        std::cerr << "❌ Failed to initialize Whisper API" << std::endl;
        return;
    }
    
    std::system("say \"Voice assistant ready. Say hello assistant to wake me up.\"");
    
    while (true) {
        std::cout << "🎤 Listening... (3 seconds)" << std::endl;
        
        SimpleAudioInput audio = RecordAudio(3.0f);
        
        if (!audio.samples.empty()) {
            SimpleSTTResult result = stt.ProcessAudio(audio);
            
            if (result.success && !result.text.empty()) {
                std::cout << "👂 Heard: \"" << result.text << "\"" << std::endl;
                
                // Simple wake word detection
                std::string lower_text = result.text;
                std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
                
                if (lower_text.find("hello assistant") != std::string::npos) {
                    std::cout << "🤖 Wake word detected!" << std::endl;
                    
                    // Extract command
                    size_t pos = lower_text.find("hello assistant");
                    if (pos != std::string::npos) {
                        std::string command = result.text.substr(pos + 15); // Length of "hello assistant"
                        
                        // Trim whitespace
                        command.erase(0, command.find_first_not_of(" \t\n\r"));
                        command.erase(command.find_last_not_of(" \t\n\r") + 1);
                        
                        if (!command.empty()) {
                            HandleCommand(command);
                        } else {
                            std::system("say \"Yes, I'm listening. How can I help you?\"");
                        }
                    }
                }
            }
        }
    }
}

void HandleCommand(const std::string& command) {
    std::string lower_command = command;
    std::transform(lower_command.begin(), lower_command.end(), lower_command.begin(), ::tolower);
    
    std::cout << "🤖 Processing command: " << command << std::endl;
    
    if (lower_command.find("hello") != std::string::npos) {
        std::system("say \"Hello! I'm using the OpenAI Whisper API for speech recognition.\"");
    } else if (lower_command.find("time") != std::string::npos) {
        auto now = std::time(nullptr);
        auto* tm = std::localtime(&now);
        char time_str[100];
        std::strftime(time_str, sizeof(time_str), "The current time is %I:%M %p", tm);
        std::string cmd = "say \"" + std::string(time_str) + "\"";
        std::system(cmd.c_str());
    } else if (lower_command.find("weather") != std::string::npos) {
        std::system("say \"I don't have access to weather information yet, but the OpenAI API is working great!\"");
    } else if (lower_command.find("stop") != std::string::npos || lower_command.find("quit") != std::string::npos) {
        std::system("say \"Goodbye!\"");
        exit(0);
    } else {
        std::string response = "say \"I heard you say: " + command + ". I'm using OpenAI Whisper API for recognition.\"";
        std::system(response.c_str());
    }
}

int main() {
    std::cout << "=" << std::string(50, '=') << std::endl;
    std::cout << "🤖 OpenAI Whisper API Test Program" << std::endl;
    std::cout << "=" << std::string(50, '=') << std::endl;
    
    std::cout << "\n📋 Prerequisites:" << std::endl;
    std::cout << "• OpenAI API key" << std::endl;
    std::cout << "• curl (for API requests)" << std::endl;
    std::cout << "• ffmpeg (for audio processing)" << std::endl;
    std::cout << "\n💡 Set your API key:" << std::endl;
    std::cout << "export OPENAI_API_KEY=your_key_here" << std::endl;
    
    std::cout << "\nChoose a test:" << std::endl;
    std::cout << "1. Test OpenAI Whisper API STT" << std::endl;
    std::cout << "2. Voice Assistant Demo with API" << std::endl;
    std::cout << "0. Exit" << std::endl;
    
    int choice;
    std::cout << "\nEnter choice: ";
    std::cin >> choice;
    std::cin.ignore(); // Consume newline
    
    switch (choice) {
        case 1:
            TestAPISTT();
            break;
        case 2:
            VoiceAssistantDemo();
            break;
        case 0:
            std::cout << "Goodbye!" << std::endl;
            break;
        default:
            std::cout << "Invalid choice" << std::endl;
            break;
    }
    
    return 0;
}
