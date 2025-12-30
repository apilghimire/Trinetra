#!/bin/bash

# Quick test script for Hobot TTS

set -e

echo "Testing Hobot TTS system..."

# Check if ROS2 is available
if ! command -v ros2 >/dev/null 2>&1; then
    echo "Error: ROS2 not found. Please source your ROS2 setup file."
    exit 1
fi

# Test basic TTS engines
echo "Testing TTS engines:"

# Test macOS say command
if command -v say >/dev/null 2>&1; then
    echo "Testing say command..."
    say "Testing macOS text to speech" &
    wait
    echo "✓ say command works"
else
    echo "✗ say command not available"
fi

# Test Festival if available
if command -v festival >/dev/null 2>&1; then
    echo "Testing Festival..."
    echo "Testing festival text to speech" | festival --tts 2>/dev/null &
    wait
    echo "✓ Festival works"
else
    echo "○ Festival not available"
fi

# Test eSpeak if available
if command -v espeak >/dev/null 2>&1; then
    echo "Testing eSpeak..."
    espeak "Testing eSpeak text to speech" 2>/dev/null &
    wait
    echo "✓ eSpeak works"
else
    echo "○ eSpeak not available"
fi

# Test audio playback
echo "Testing audio playback tools:"

if command -v afplay >/dev/null 2>&1; then
    echo "✓ afplay available"
elif command -v aplay >/dev/null 2>&1; then
    echo "✓ aplay available"
else
    echo "✗ No audio player found"
fi

echo ""
echo "System test completed!"
echo ""

# Test ROS2 functionality if the node is built
if [ -f "install/lib/hobot_tts_macos/hobot_tts_node" ] || [ -f "build/hobot_tts_node" ]; then
    echo "TTS node executable found. You can test the full system with:"
    echo ""
    echo "Terminal 1:"
    echo "  source install/setup.bash"
    echo "  ros2 run hobot_tts_macos hobot_tts_node"
    echo ""
    echo "Terminal 2:"
    echo "  source install/setup.bash"
    echo "  ros2 topic pub --once /tts_text std_msgs/msg/String '{data: \"Hello from ROS2!\"}'"
else
    echo "TTS node not built yet. Run the build first:"
    echo "  colcon build --packages-select hobot_tts_macos"
fi
