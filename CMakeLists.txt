cmake_minimum_required(VERSION 3.8)
project(hobot_tts_macos)

# Compiler settings
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

# Find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)

# Include directories
include_directories(include)

# macOS-specific settings
if(APPLE)
  find_library(CORE_AUDIO_FRAMEWORK CoreAudio)
  find_library(AUDIO_UNIT_FRAMEWORK AudioUnit)
  find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox)
  set(MACOS_FRAMEWORKS ${CORE_AUDIO_FRAMEWORK} ${AUDIO_UNIT_FRAMEWORK} ${AUDIO_TOOLBOX_FRAMEWORK})
endif()

# Source files
set(SOURCES
  src/main.cpp
  src/hobot_tts_node.cpp
  src/tts_engine.cpp
  src/audio_manager.cpp
)

# Create executable
add_executable(hobot_tts_node ${SOURCES})

# Link dependencies
ament_target_dependencies(hobot_tts_node 
  rclcpp 
  std_msgs
)

# Link macOS frameworks
if(APPLE)
  target_link_libraries(hobot_tts_node ${MACOS_FRAMEWORKS})
endif()

# Install targets
install(TARGETS
  hobot_tts_node
  DESTINATION lib/${PROJECT_NAME}
)

# Install launch files
install(DIRECTORY
  launch
  DESTINATION share/${PROJECT_NAME}/
)

# Install config files
install(DIRECTORY
  config
  DESTINATION share/${PROJECT_NAME}/
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
