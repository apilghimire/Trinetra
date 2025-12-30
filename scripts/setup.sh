#!/bin/bash

# Setup script for Hobot TTS macOS

set -e

echo "Setting up Hobot TTS for macOS..."

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check ROS2 installation
if ! command_exists ros2; then
    echo "Error: ROS2 not found. Please install ROS2 first."
    echo "You can install it using: brew install ros"
    exit 1
fi

echo "✓ ROS2 found"

# Check for TTS engines
echo "Checking TTS engines..."

if command_exists say; then
    echo "✓ macOS say command available"
    echo "Available voices:"
    say -v ? | head -5
    echo "..."
else
    echo "✗ macOS say command not found (this is unusual for macOS)"
fi

if command_exists festival; then
    echo "✓ Festival TTS available"
else
    echo "○ Festival TTS not found (optional)"
    echo "  Install with: brew install festival"
fi

if command_exists espeak; then
    echo "✓ eSpeak TTS available"
else
    echo "○ eSpeak TTS not found (optional)"
    echo "  Install with: brew install espeak"
fi

# Check audio tools
echo "Checking audio tools..."

if command_exists afplay; then
    echo "✓ afplay available"
elif command_exists aplay; then
    echo "✓ aplay available"
else
    echo "✗ No audio player found"
    exit 1
fi

if command_exists afconvert; then
    echo "✓ afconvert available"
else
    echo "○ afconvert not found (audio conversion may be limited)"
fi

# Build the project
echo "Building project..."
if [ -f "CMakeLists.txt" ]; then
    mkdir -p build
    cd build
    cmake ..
    make -j$(sysctl -n hw.ncpu)
    cd ..
    echo "✓ Build completed"
else
    echo "Using ROS2 colcon build..."
    if command_exists colcon; then
        colcon build --packages-select hobot_tts_macos
        echo "✓ Build completed with colcon"
    else
        echo "Error: Neither CMake nor colcon build method available"
        exit 1
    fi
fi

echo ""
echo "Setup completed successfully!"
echo ""
echo "To use the system:"
echo "1. Source the ROS2 environment:"
if [ -f "install/setup.bash" ]; then
    echo "   source install/setup.bash"
else
    echo "   source /opt/ros/humble/setup.bash  # or your ROS2 installation"
fi
echo ""
echo "2. Start the TTS node:"
echo "   ros2 run hobot_tts_macos hobot_tts_node"
echo ""
echo "3. Send a test message (in another terminal):"
echo "   ros2 topic pub --once /tts_text std_msgs/msg/String '{data: \"Hello, world!\"}'"
echo ""
echo "For more options, see the README.md file."
