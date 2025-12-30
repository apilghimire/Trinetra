#!/bin/bash

# Build script for OpenAI Whisper API test

echo "🔨 Building OpenAI Whisper API test..."

cd "$(dirname "$0")"

# Check for required tools
if ! command -v clang++ &> /dev/null; then
    echo "❌ clang++ not found. Please install Xcode command line tools:"
    echo "   xcode-select --install"
    exit 1
fi

if ! command -v curl &> /dev/null; then
    echo "❌ curl not found. Install with: brew install curl"
    exit 1
fi

if ! command -v ffmpeg &> /dev/null; then
    echo "❌ ffmpeg not found. Install with: brew install ffmpeg"
    exit 1
fi

# Build the test
echo "Compiling test_whisper_api.cpp..."
clang++ -std=c++14 -O2 -o test_whisper_api test_whisper_api.cpp

if [ $? -eq 0 ]; then
    echo "✅ Build successful!"
    echo ""
    echo "🔑 Setup your API key:"
    echo "   export OPENAI_API_KEY=your_api_key_here"
    echo ""
    echo "🚀 Run the test:"
    echo "   ./test_whisper_api"
    echo ""
    echo "🌟 Features:"
    echo "• Uses OpenAI Whisper API (cloud-based)"
    echo "• Higher accuracy than local models"
    echo "• Faster processing (no local model loading)"
    echo "• Always up-to-date with latest improvements"
    echo "• Supports 50+ languages"
    echo ""
    echo "💰 Cost: ~$0.006 per minute of audio"
else
    echo "❌ Build failed"
    exit 1
fi
