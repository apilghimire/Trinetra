// Copyright (c) 2024, Adapted from D-Robotics Hobot TTS.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hobot_tts_node.h"
#include "rclcpp/rclcpp.hpp"

int main(int argc, char* argv[]) {
  rclcpp::init(argc, argv);
  
  auto nh = std::make_shared<rclcpp::Node>("hobot_tts_node");
  
  try {
    hobot_tts::HobotTTSNode tts_node(nh);
    rclcpp::spin(nh);
  } catch (const std::exception& e) {
    RCLCPP_ERROR(nh->get_logger(), "Failed to initialize TTS node: %s", e.what());
    return 1;
  }
  
  rclcpp::shutdown();
  return 0;
}
