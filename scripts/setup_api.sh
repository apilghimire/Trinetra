#!/bin/bash

# Quick setup script for OpenAI Whisper API testing

echo "🔑 OpenAI Whisper API Setup"
echo "=========================="

# Check if API key is provided as argument
if [ $# -eq 1 ]; then
    API_KEY="$1"
    echo "✓ Using provided API key"
elif [ -n "$OPENAI_API_KEY" ]; then
    echo "✓ Using API key from environment variable"
    API_KEY="$OPENAI_API_KEY"
else
    echo "❌ No API key found"
    echo ""
    echo "Usage:"
    echo "  ./setup_api.sh your-api-key-here"
    echo "  OR"
    echo "  export OPENAI_API_KEY=your-api-key-here && ./setup_api.sh"
    echo ""
    echo "Get your API key at: https://platform.openai.com/api-keys"
    exit 1
fi

# Set the environment variable for this session
export OPENAI_API_KEY="$API_KEY"

# Check dependencies
echo ""
echo "🔍 Checking dependencies..."

# Check curl
if ! command -v curl &> /dev/null; then
    echo "❌ curl not found. Install with: brew install curl"
    exit 1
else
    echo "✓ curl found"
fi

# Check ffmpeg
if ! command -v ffmpeg &> /dev/null; then
    echo "❌ ffmpeg not found. Install with: brew install ffmpeg"
    exit 1
else
    echo "✓ ffmpeg found"
fi

# Check Python and requests
echo "🐍 Checking Python setup..."
if ! command -v python3 &> /dev/null; then
    echo "❌ python3 not found"
    exit 1
else
    echo "✓ python3 found"
fi

# Test API key with a simple request
echo ""
echo "🧪 Testing API key..."

TEST_RESPONSE=$(curl -s -o /dev/null -w "%{http_code}" -X GET "https://api.openai.com/v1/models" \
  -H "Authorization: Bearer $API_KEY")

if [ "$TEST_RESPONSE" -eq 200 ]; then
    echo "✅ API key is valid!"
else
    echo "❌ API key test failed (HTTP $TEST_RESPONSE)"
    echo "Please check your API key and internet connection"
    exit 1
fi

# Install Python requests if needed
echo ""
echo "📦 Checking Python packages..."
python3 -c "import requests" 2>/dev/null
if [ $? -eq 0 ]; then
    echo "✓ requests library found"
else
    echo "Installing requests library..."
    pip3 install requests
    if [ $? -eq 0 ]; then
        echo "✓ requests library installed"
    else
        echo "❌ Failed to install requests library"
        echo "Try: pip3 install --user requests"
    fi
fi

# Build C++ test program
echo ""
echo "🔨 Building test programs..."

cd "$(dirname "$0")"
if [ -f "../simple_build/build_whisper_api.sh" ]; then
    cd ../simple_build
    ./build_whisper_api.sh
    if [ $? -eq 0 ]; then
        echo "✓ C++ test program built"
    else
        echo "❌ C++ build failed"
    fi
    cd - > /dev/null
else
    echo "⚠️  C++ build script not found"
fi

# Summary
echo ""
echo "🎉 Setup Complete!"
echo "=================="
echo ""
echo "🚀 Quick Tests:"
echo "  1. Python demo:  python3 ../scripts/demo_whisper_api.py"
echo "  2. C++ program:   ../simple_build/test_whisper_api" 
echo ""
echo "💡 API key is set for this session: \$OPENAI_API_KEY"
echo "💡 To make it permanent: echo 'export OPENAI_API_KEY=\"$API_KEY\"' >> ~/.zshrc"
echo ""
echo "📝 Cost: ~$0.006 per minute of audio"
echo "📚 Full guide: cat ../docs/OPENAI_API_GUIDE.md"
