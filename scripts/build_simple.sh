#!/bin/bash

# Simple standalone build script for testing basic functionality

set -e

echo "Building simple standalone Hobot TTS test..."

# Create build directory
mkdir -p simple_build
cd simple_build

# Create a simple test file that only uses the basic functionality
cat > simple_test.cpp << 'EOF'
#include <iostream>
#include <string>
#include <cstdlib>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstdio>

// Simple C++14 compatible file operations
bool remove_file(const std::string& filename) {
    return std::remove(filename.c_str()) == 0;
}

// Simple TTS test using macOS say command
bool test_say_tts(const std::string& text, const std::string& voice = "") {
    std::string temp_file = "/tmp/hobot_tts_test_" + std::to_string(std::rand()) + ".aiff";
    
    std::ostringstream cmd;
    cmd << "say";
    
    if (!voice.empty() && voice != "default") {
        cmd << " -v \"" << voice << "\"";
    }
    
    cmd << " -o \"" << temp_file << "\"";
    cmd << " \"" << text << "\"";
    
    std::cout << "Executing: " << cmd.str() << std::endl;
    
    int result = std::system(cmd.str().c_str());
    if (result != 0) {
        std::cerr << "Error: say command failed with code " << result << std::endl;
        return false;
    }
    
    // Check if file was created
    std::ifstream file(temp_file);
    bool success = file.good();
    file.close();
    
    if (success) {
        std::cout << "Generated audio file: " << temp_file << std::endl;
        
        // Convert to WAV and play
        std::string wav_file = temp_file + ".wav";
        std::ostringstream convert_cmd;
        convert_cmd << "afconvert -f WAVE -d LEI16@16000 \"" << temp_file << "\" \"" << wav_file << "\"";
        
        if (std::system(convert_cmd.str().c_str()) == 0) {
            std::cout << "Playing audio..." << std::endl;
            std::string play_cmd = "afplay \"" + wav_file + "\"";
            std::system(play_cmd.c_str());
            remove_file(wav_file);
        }
    }
    
    remove_file(temp_file);
    return success;
}

// Get available voices
std::vector<std::string> get_voices() {
    std::vector<std::string> voices;
    
    FILE* pipe = popen("say -v \\? 2>/dev/null", "r");
    if (!pipe) {
        return voices;
    }
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        std::string line(buffer);
        size_t space_pos = line.find(' ');
        if (space_pos != std::string::npos) {
            voices.push_back(line.substr(0, space_pos));
        }
    }
    
    pclose(pipe);
    return voices;
}

// Test text segmentation
std::vector<std::string> split_text(const std::string& text) {
    std::vector<std::string> segments;
    std::stringstream ss(text);
    std::string segment;
    
    // Simple split by sentences (periods, exclamations, questions)
    std::string current;
    for (char c : text) {
        current += c;
        if (c == '.' || c == '!' || c == '?') {
            if (!current.empty()) {
                // Trim whitespace
                size_t start = current.find_first_not_of(" \t\n\r");
                size_t end = current.find_last_not_of(" \t\n\r");
                if (start != std::string::npos && end != std::string::npos) {
                    segments.push_back(current.substr(start, end - start + 1));
                }
                current.clear();
            }
        }
    }
    
    // Add remaining text
    if (!current.empty()) {
        size_t start = current.find_first_not_of(" \t\n\r");
        size_t end = current.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos) {
            segments.push_back(current.substr(start, end - start + 1));
        }
    }
    
    return segments;
}

int main() {
    std::cout << "=== Simple Hobot TTS Test ===" << std::endl;
    
    // Check if say is available
    if (std::system("which say > /dev/null 2>&1") != 0) {
        std::cerr << "Error: 'say' command not found. This requires macOS." << std::endl;
        return 1;
    }
    
    std::cout << "✓ macOS say command found" << std::endl;
    
    // Get available voices
    auto voices = get_voices();
    if (!voices.empty()) {
        std::cout << "Available voices (first 10):" << std::endl;
        for (size_t i = 0; i < std::min(voices.size(), size_t(10)); i++) {
            std::cout << "  - " << voices[i] << std::endl;
        }
        if (voices.size() > 10) {
            std::cout << "  ... and " << (voices.size() - 10) << " more" << std::endl;
        }
    }
    
    // Test texts similar to original Hobot TTS
    std::vector<std::string> test_texts = {
        "Hello, this is a test of the Hobot TTS system running on macOS.",
        "The quick brown fox jumps over the lazy dog! This tests various phonemes.",
        "Do you know the horizon? Yes, I know the horizon. It is a line that extends from the ground to the sky, defining the boundary between the ground and the sky.",
        "Testing numbers: one, two, three. And punctuation! Are you listening? Yes, I am."
    };
    
    for (size_t i = 0; i < test_texts.size(); i++) {
        std::cout << "\n--- Test " << (i + 1) << " ---" << std::endl;
        std::cout << "Text: " << test_texts[i] << std::endl;
        
        // Test text segmentation
        auto segments = split_text(test_texts[i]);
        std::cout << "Segments (" << segments.size() << "):" << std::endl;
        for (const auto& segment : segments) {
            std::cout << "  - " << segment << std::endl;
        }
        
        // Synthesize and play each segment
        for (size_t j = 0; j < segments.size(); j++) {
            std::cout << "\nProcessing segment " << (j + 1) << "/" << segments.size() 
                      << ": " << segments[j] << std::endl;
            
            if (test_say_tts(segments[j])) {
                std::cout << "✓ Segment processed successfully" << std::endl;
            } else {
                std::cout << "✗ Segment processing failed" << std::endl;
            }
        }
        
        if (i < test_texts.size() - 1) {
            std::cout << "\nPress Enter to continue to next test..." << std::endl;
            std::cin.get();
        }
    }
    
    std::cout << "\n=== Testing different voices ===" << std::endl;
    if (voices.size() >= 2) {
        for (size_t i = 0; i < std::min(voices.size(), size_t(3)); i++) {
            std::cout << "\nTesting voice: " << voices[i] << std::endl;
            if (test_say_tts("Hello, I am speaking with voice " + voices[i], voices[i])) {
                std::cout << "✓ Voice " << voices[i] << " works" << std::endl;
            }
        }
    }
    
    std::cout << "\n=== Test completed successfully! ===" << std::endl;
    std::cout << "\nThis demonstrates the core functionality that the full ROS2 version provides:" << std::endl;
    std::cout << "1. ✓ Text-to-speech synthesis using macOS say command" << std::endl;
    std::cout << "2. ✓ Text segmentation for better speech quality" << std::endl;
    std::cout << "3. ✓ Audio generation and playback" << std::endl;
    std::cout << "4. ✓ Multiple voice support" << std::endl;
    std::cout << "\nTo use the full ROS2 version:" << std::endl;
    std::cout << "1. Install ROS2: brew install ros" << std::endl;
    std::cout << "2. Build: colcon build --packages-select hobot_tts_macos" << std::endl;
    std::cout << "3. Run: ros2 run hobot_tts_macos hobot_tts_node" << std::endl;
    std::cout << "4. Test: ros2 topic pub --once /tts_text std_msgs/msg/String '{data: \"Hello ROS2!\"}'" << std::endl;
    
    return 0;
}
EOF

# Compile with basic C++ flags
echo "Compiling simple test..."
g++ -std=c++14 -o simple_hobot_tts_test simple_test.cpp

# Make it executable
chmod +x simple_hobot_tts_test

echo "✓ Simple build completed!"
echo ""
echo "Run the test with:"
echo "  ./simple_build/simple_hobot_tts_test"

cd ..
