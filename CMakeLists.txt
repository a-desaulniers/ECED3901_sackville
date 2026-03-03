cmake_minimum_required(VERSION 3.8)
project(eced3901)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclpy REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(nav_msgs REQUIRED)
find_package(tf2 REQUIRED)

# --- Cpp executables ---
add_executable(talker src/publisher_member_function.cpp)
ament_target_dependencies(talker rclcpp std_msgs)

add_executable(dt1 src/eced3901_dt1.cpp)
ament_target_dependencies(dt1 rclcpp std_msgs nav_msgs geometry_msgs tf2)

add_executable(RTest src/Robot_Test.cpp)
ament_target_dependencies(RTest rclcpp std_msgs nav_msgs geometry_msgs tf2)

# New Serial Bridge Executable
add_executable(serial_bridge src/serial_bridge_node.cpp)
ament_target_dependencies(serial_bridge rclcpp std_msgs)

# --- Install Cpp executables ---
install(TARGETS
  talker
  dt1
  RTest
  serial_bridge
  DESTINATION lib/${PROJECT_NAME})

install(
  DIRECTORY config launch maps models params rviz src worlds
  DESTINATION share/${PROJECT_NAME})

# --- Install Python executables ---
install(PROGRAMS
  scripts/demo_inspection.py
  DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
