#!/bin/bash

# Installation script for Hobot TTS+STT dependencies

echo "🤖 Hobot TTS+STT Dependency Installer"
echo "======================================"

# Check if we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    echo "❌ This script is designed for macOS"
    exit 1
fi

# Check if Homebrew is installed
if ! command -v brew &> /dev/null; then
    echo "❌ Homebrew not found. Please install Homebrew first:"
    echo "   /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
    exit 1
fi

echo "✓ macOS and Homebrew detected"

# Install audio tools
echo ""
echo "📦 Installing audio tools..."

if ! command -v ffmpeg &> /dev/null; then
    echo "Installing FFmpeg..."
    brew install ffmpeg
    if [ $? -eq 0 ]; then
        echo "✓ FFmpeg installed"
    else
        echo "❌ FFmpeg installation failed"
        exit 1
    fi
else
    echo "✓ FFmpeg already installed"
fi

if ! command -v sox &> /dev/null; then
    echo "Installing SoX (optional but recommended)..."
    brew install sox
    if [ $? -eq 0 ]; then
        echo "✓ SoX installed"
    else
        echo "⚠️  SoX installation failed (optional, continuing...)"
    fi
else
    echo "✓ SoX already installed"
fi

# Check Python
echo ""
echo "🐍 Checking Python..."

if ! command -v python3 &> /dev/null; then
    echo "❌ Python 3 not found. Please install Python 3:"
    echo "   brew install python"
    exit 1
fi

python_version=$(python3 --version | cut -d' ' -f2 | cut -d'.' -f1-2)
echo "✓ Python $python_version found"

# Install Python packages
echo ""
echo "📦 Installing Python packages..."

# Check if pip is available
if ! command -v pip3 &> /dev/null; then
    echo "❌ pip3 not found. Please install pip:"
    echo "   python3 -m ensurepip --upgrade"
    exit 1
fi

# Install whisper
echo "Installing OpenAI Whisper..."
pip3 install -q openai-whisper

if [ $? -eq 0 ]; then
    echo "✓ OpenAI Whisper installed"
else
    echo "❌ OpenAI Whisper installation failed"
    exit 1
fi

# Test whisper installation
echo "Testing Whisper installation..."
python3 -c "import whisper; print('Whisper import successful')" 2>/dev/null

if [ $? -eq 0 ]; then
    echo "✓ Whisper installation verified"
else
    echo "❌ Whisper installation verification failed"
    exit 1
fi

# Check C++ compiler
echo ""
echo "🔨 Checking C++ compiler..."

if ! command -v clang++ &> /dev/null; then
    echo "❌ clang++ not found. Please install Xcode command line tools:"
    echo "   xcode-select --install"
    exit 1
fi

echo "✓ clang++ found"

# Test TTS
echo ""
echo "🗣️  Testing TTS..."
say "Text to speech is working" 2>/dev/null

if [ $? -eq 0 ]; then
    echo "✓ macOS TTS working"
else
    echo "❌ macOS TTS failed"
    exit 1
fi

# Test recording capability
echo ""
echo "🎤 Testing recording capability..."

# Test ffmpeg recording (silent test)
timeout 1 ffmpeg -y -loglevel quiet -f avfoundation -i ":0" -t 0.1 /tmp/test_record.wav 2>/dev/null

if [ -f /tmp/test_record.wav ]; then
    echo "✓ Recording capability verified"
    rm -f /tmp/test_record.wav
else
    echo "⚠️  Recording test inconclusive (may need microphone permissions)"
fi

# ROS2 check (optional)
echo ""
echo "🤖 Checking ROS2 (optional)..."

if command -v ros2 &> /dev/null; then
    echo "✓ ROS2 found"
    echo "  You can use the full ROS2 integration"
else
    echo "⚠️  ROS2 not found (optional)"
    echo "  You can still use the standalone builds"
fi

# Summary
echo ""
echo "🎉 Installation Summary"
echo "======================"
echo ""
echo "✅ Core Dependencies:"
echo "   • FFmpeg - Audio recording/processing"
echo "   • Python 3 - Runtime environment"
echo "   • OpenAI Whisper - Speech recognition"
echo "   • clang++ - C++ compilation"
echo "   • macOS TTS - Speech synthesis"
echo ""

if command -v sox &> /dev/null; then
    echo "✅ Optional Dependencies:"
    echo "   • SoX - Enhanced audio recording"
    echo ""
fi

echo "🚀 Quick Start:"
echo "   1. Build STT test:    cd simple_build && ./build_stt.sh"
echo "   2. Run TTS test:      echo '1' | ./test_stt"
echo "   3. Run STT test:      echo '2' | ./test_stt"
echo "   4. Voice assistant:   echo '3' | ./test_stt"
echo ""
echo "📚 Documentation:      cat docs/STT_SYSTEM.md"
echo ""
echo "🎤 Ready for voice processing!"

# Create a simple test command
echo ""
echo "Creating quick test command..."
cat > /tmp/test_hobot_voice.sh << 'EOF'
#!/bin/bash
echo "🤖 Quick Hobot Voice Test"
echo "========================"
echo ""
echo "Testing TTS..."
say "Hello, this is the Hobot TTS system"
echo "✓ TTS test complete"
echo ""
echo "🎤 For full testing, run:"
echo "   cd /Users/apilghimire/Documents/VLM_Test/simple_build"
echo "   ./test_stt"
EOF

chmod +x /tmp/test_hobot_voice.sh
echo "Quick test available at: /tmp/test_hobot_voice.sh"

echo ""
echo "🎯 Installation complete! Happy voice processing! 🎉"
