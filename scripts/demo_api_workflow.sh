#!/bin/bash

# Demo script showing OpenAI Whisper API usage (without actual API calls)
# This script demonstrates the workflow and commands

echo "🤖 OpenAI Whisper API Demo Workflow"
echo "===================================="

echo ""
echo "📋 Prerequisites Check:"
echo "----------------------"

# Check tools
tools=("curl" "ffmpeg" "python3")
for tool in "${tools[@]}"; do
    if command -v "$tool" &> /dev/null; then
        echo "✅ $tool: found"
    else
        echo "❌ $tool: not found (install with: brew install $tool)"
    fi
done

echo ""
echo "🔑 API Key Setup:"
echo "-----------------"
echo "1. Get API key from: https://platform.openai.com/api-keys"
echo "2. Set environment variable:"
echo "   export OPENAI_API_KEY=\"sk-your-key-here\""
echo "3. Or provide when prompted by programs"

echo ""
echo "🎤 Recording Workflow:"
echo "----------------------"

echo "Step 1: Record audio (5 seconds)"
echo "Command: ffmpeg -f avfoundation -i \":0\" -t 5 -ar 16000 -ac 1 audio.wav"

echo ""
echo "Step 2: Convert to MP3 (API preferred format)"
echo "Command: ffmpeg -i audio.wav -acodec libmp3lame -b:a 64k audio.mp3"

echo ""
echo "Step 3: Send to OpenAI API"
echo "Command:"
cat << 'EOF'
curl -X POST "https://api.openai.com/v1/audio/transcriptions" \
  -H "Authorization: Bearer $OPENAI_API_KEY" \
  -H "Content-Type: multipart/form-data" \
  -F "file=@audio.mp3" \
  -F "model=whisper-1" \
  -F "response_format=json"
EOF

echo ""
echo "📤 API Response Example:"
echo "------------------------"
cat << 'EOF'
{
  "text": "Hello, this is a test of the OpenAI Whisper API speech recognition system."
}
EOF

echo ""
echo "🔄 Complete Example (Python):"
echo "------------------------------"
cat << 'EOF'
import requests
import os

def transcribe_audio(audio_file):
    api_key = os.getenv('OPENAI_API_KEY')
    url = "https://api.openai.com/v1/audio/transcriptions"
    
    headers = {"Authorization": f"Bearer {api_key}"}
    files = {"file": open(audio_file, "rb")}
    data = {"model": "whisper-1"}
    
    response = requests.post(url, headers=headers, files=files, data=data)
    return response.json()["text"]

# Usage:
result = transcribe_audio("audio.mp3")
print(f"Transcription: {result}")
EOF

echo ""
echo "🔄 Complete Example (C++):"
echo "---------------------------"
cat << 'EOF'
// Using curl in C++
std::string transcribe_audio(const std::string& audio_file) {
    std::string cmd = "curl -s -X POST "
                     "\"https://api.openai.com/v1/audio/transcriptions\" "
                     "-H \"Authorization: Bearer " + api_key + "\" "
                     "-F \"file=@" + audio_file + "\" "
                     "-F \"model=whisper-1\"";
    
    FILE* pipe = popen(cmd.c_str(), "r");
    // ... parse JSON response
}
EOF

echo ""
echo "💰 Pricing Information:"
echo "------------------------"
echo "• Cost: $0.006 per minute of audio"
echo "• Billing: Per-second granularity"
echo "• Examples:"
echo "  - 10 voice commands/day (30s each): ~$0.05/month"
echo "  - 1 hour of audio/day: ~$10.80/month"
echo "  - 10 minutes/day: ~$1.80/month"

echo ""
echo "🎯 Available Test Programs:"
echo "----------------------------"

if [ -f "/Users/apilghimire/Documents/VLM_Test/simple_build/test_whisper_api" ]; then
    echo "✅ C++ Test Program: ./simple_build/test_whisper_api"
else
    echo "❌ C++ Test Program: ./simple_build/test_whisper_api (not built)"
    echo "   Build with: ./simple_build/build_whisper_api.sh"
fi

if [ -f "/Users/apilghimire/Documents/VLM_Test/scripts/demo_whisper_api.py" ]; then
    echo "✅ Python Demo: ./scripts/demo_whisper_api.py"
else
    echo "❌ Python Demo: ./scripts/demo_whisper_api.py (not found)"
fi

echo ""
echo "🌟 Advantages of OpenAI API vs Local:"
echo "-------------------------------------"
echo "✅ No model downloads (0 setup time)"
echo "✅ Higher accuracy (latest models)"  
echo "✅ Faster processing (no model loading)"
echo "✅ 50+ languages with better detection"
echo "✅ Always up-to-date"
echo "✅ Lower resource usage"
echo "✅ Professional-grade reliability"

echo ""
echo "🚀 Quick Start:"
echo "---------------"
echo "1. Get API key: https://platform.openai.com/api-keys"
echo "2. Set environment: export OPENAI_API_KEY=\"your-key\""
echo "3. Install requests: pip3 install requests"
echo "4. Run test: python3 scripts/demo_whisper_api.py"

echo ""
echo "📚 Full Documentation: docs/OPENAI_API_GUIDE.md"
echo "🎤 Ready for high-quality speech recognition!"
