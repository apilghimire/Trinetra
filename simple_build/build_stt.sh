#!/bin/bash

# Build script for STT test

echo "🔨 Building STT test..."

cd "$(dirname "$0")"

# Check for required tools
if ! command -v clang++ &> /dev/null; then
    echo "❌ clang++ not found. Please install Xcode command line tools:"
    echo "   xcode-select --install"
    exit 1
fi

# Check for recording tools
if ! command -v ffmpeg &> /dev/null && ! command -v sox &> /dev/null; then
    echo "⚠️  No recording tools found. Install ffmpeg or sox:"
    echo "   brew install ffmpeg"
    echo "   brew install sox"
    echo "Continuing anyway (some features may not work)..."
fi

# Build the test
echo "Compiling test_stt.cpp..."
clang++ -std=c++14 -O2 -o test_stt test_stt.cpp

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Run the test with: ./test_stt"
    echo ""
    echo "🤖 STT Test Features:"
    echo "1. Text-to-Speech testing with macOS 'say' command"
    echo "2. Speech-to-Text testing with OpenAI Whisper"
    echo "3. Voice Assistant demo with wake word detection"
    echo ""
    echo "📋 Prerequisites:"
    echo "- Python 3 with pip"
    echo "- OpenAI Whisper (will be auto-installed)"
    echo "- FFmpeg or SoX for recording"
else
    echo "❌ Build failed"
    exit 1
fi
