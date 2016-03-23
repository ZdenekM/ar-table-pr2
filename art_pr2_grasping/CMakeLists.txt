cmake_minimum_required(VERSION 2.8.3)
project(art_pr2_grasping)

find_package(catkin REQUIRED COMPONENTS 
  moveit_core 
  moveit_ros_planning 
  moveit_ros_planning_interface 
  moveit_visual_tools
  moveit_simple_grasps
  std_msgs
  geometry_msgs
  roscpp
  cmake_modules
  pr2_controllers_msgs
  actionlib
  actionlib_msgs
  shape_msgs
)

find_package(Eigen REQUIRED)
#find_package(Boost REQUIRED system thread)

add_action_files(
  DIRECTORY action
  FILES pickplace.action
)

generate_messages(
  DEPENDENCIES actionlib_msgs shape_msgs
)

catkin_package(
  INCLUDE_DIRS include
  CATKIN_DEPENDS 
    moveit_simple_grasps
    actionlib_msgs
)

# Enable gdb to show line numbers
#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g")
#set(CMAKE_BUILD_TYPE Debug)

###########
## Build ##
###########

#include_directories(include ${catkin_INCLUDE_DIRS} ${Boost_INCLUDE_DIRS})
include_directories(include ${catkin_INCLUDE_DIRS})

# Main executable
add_executable(pr2grasp src/pr2grasp.cpp)
#target_link_libraries(pr2grasp ${catkin_LIBRARIES} ${Boost_LIBRARIES})
target_link_libraries(pr2grasp ${catkin_LIBRARIES})
