cmake_minimum_required(VERSION 2.8.3)
project(f1tenth_iv)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  std_msgs
  sensor_msgs
  ackermann_msgs
)

catkin_package(
 LIBRARIES ${PROJECT_NAME}
 CATKIN_DEPENDS roscpp std_msgs sensor_msgs ackermann_msgs
)

include_directories(${catkin_INCLUDE_DIRS})

add_executable(reactive_gap_follower src/reactive_gap_follower.cpp)
target_link_libraries(reactive_gap_follower ${catkin_LIBRARIES})
