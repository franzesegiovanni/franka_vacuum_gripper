// Copyright (c) 2017 Franka Emika GmbH
// Use of this source code is governed by the Apache-2.0 license, see LICENSE
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ros/init.h>
#include <ros/node_handle.h>
#include <ros/rate.h>
#include <ros/spinner.h>

#include <franka/vacuum_gripper_state.h>
#include <franka_vacuum_gripper/franka_vacuum_gripper.h>

#include <franka_vacuum_gripper/StopAction.h>
#include <franka_vacuum_gripper/VacuumAction.h>
#include <franka_vacuum_gripper/DropOffAction.h>

#include <franka_vacuum_gripper/VacuumState.h>

namespace {

template <typename T_action, typename T_goal, typename T_result>
void handleErrors(actionlib::SimpleActionServer<T_action>* server,
                  std::function<bool(const T_goal&)> handler,
                  const T_goal& goal) {
  T_result result;
  try {
    result.success = handler(goal);
    server->setSucceeded(result);
  } catch (const franka::Exception& ex) {
    ROS_ERROR_STREAM("" << ex.what());
    result.success = false;
    result.error = ex.what();
    server->setAborted(result);
  }
}

}  // anonymous namespace

using actionlib::SimpleActionServer;

using franka_vacuum_gripper::vacuum;
using franka_vacuum_gripper::VacuumAction;

using franka_vacuum_gripper::stop;
using franka_vacuum_gripper::StopAction;
using franka_vacuum_gripper::StopGoalConstPtr;
using franka_vacuum_gripper::StopResult;

using franka_vacuum_gripper::dropOff;
using franka_vacuum_gripper::DropOffAction;

using franka_vacuum_gripper::updateGripperState;

int main(int argc, char** argv) {
  ros::init(argc, argv, "franka_vacuum_gripper_node");
  ros::NodeHandle node_handle("~");
  std::string robot_ip;
  if (!node_handle.getParam("robot_ip", robot_ip)) {
    ROS_ERROR("franka_gripper_node: Could not parse robot_ip parameter");
    return -1;
  }

  franka::VacuumGripper gripper(robot_ip);

  auto stop_handler = [&gripper](auto&& goal) { return stop(gripper, goal); };
  auto vacuum_handler = [&gripper](auto&& goal) { return vacuum(gripper, goal); };
  auto dropoff_handler = [&gripper](auto&& goal) {return dropOff(gripper, goal); }; 

  SimpleActionServer<StopAction> stop_action_server(
      node_handle, "stop",
      [=, &stop_action_server](auto&& goal) {
        return handleErrors<franka_vacuum_gripper::StopAction, franka_vacuum_gripper::StopGoalConstPtr,
                            franka_vacuum_gripper::StopResult>(&stop_action_server, stop_handler, goal);
      },
      false);

  SimpleActionServer<VacuumAction> vacuum_action_server(
      node_handle, "vacuum",
      [=, &vacuum_action_server](auto&& goal) {
        return handleErrors<franka_vacuum_gripper::VacuumAction, franka_vacuum_gripper::VacuumGoalConstPtr,
                            franka_vacuum_gripper::VacuumResult>(&vacuum_action_server, vacuum_handler, goal);
      },
      false);

  SimpleActionServer<DropOffAction> dropoff_action_server(
      node_handle, "dropoff",
      [=, &dropoff_action_server](auto&& goal) {
        return handleErrors<franka_vacuum_gripper::DropOffAction, franka_vacuum_gripper::DropOffGoalConstPtr,
                            franka_vacuum_gripper::DropOffResult>(&dropoff_action_server, dropoff_handler, goal);
      },
      false);


  stop_action_server.start();
  vacuum_action_server.start();
  dropoff_action_server.start();

  double publish_rate(30.0);
  if (!node_handle.getParam("publish_rate", publish_rate)) {
    ROS_INFO_STREAM("franka_gripper_node: Could not find parameter publish_rate. Defaulting to "
                    << publish_rate);
  }

  bool stop_at_shutdown(false);
  if (!node_handle.getParam("stop_at_shutdown", stop_at_shutdown)) {
    ROS_INFO_STREAM("franka_gripper_node: Could not find parameter stop_at_shutdown. Defaulting to "
                    << std::boolalpha << stop_at_shutdown);
  }

  franka::VacuumGripperState gripper_state;
  std::mutex gripper_state_mutex;
  std::thread read_thread([&gripper_state, &gripper, &gripper_state_mutex]() {
    ros::Rate read_rate(5);
    while (ros::ok()) {
      {
        std::lock_guard<std::mutex> read_lock (gripper_state_mutex);
        updateGripperState(gripper, &gripper_state);
      }
      read_rate.sleep();
    }
  });

  ros::AsyncSpinner spinner(2);
  spinner.start();
  std::thread publish_thread([&gripper_state, &gripper_state_mutex, &node_handle]() {
    ros::Publisher vacuum_state_publisher =
            node_handle.advertise<franka_vacuum_gripper::VacuumState>("vacuum_state", 1);
    ros::Rate rate(10);
    while (ros::ok()) {
      {
        std::lock_guard<std::mutex> pub_lock (gripper_state_mutex);
        franka_vacuum_gripper::VacuumState vacuum_state;
        vacuum_state.header.stamp = ros::Time::now();
        vacuum_state.in_control_range = gripper_state.in_control_range;
        vacuum_state.part_detached = gripper_state.part_detached;
        vacuum_state.part_present = gripper_state.part_present;

        std::string device_status;
        switch (gripper_state.device_status) {
     	  case franka::VacuumGripperDeviceStatus::kGreen:
            device_status = "Green";
            break;
	  case franka::VacuumGripperDeviceStatus::kYellow:
            device_status = "Yellow";
            break;
	  case franka::VacuumGripperDeviceStatus::kOrange:
            device_status = "Orange";
            break;
	  case franka::VacuumGripperDeviceStatus::kRed:
            device_status = "Red";
            break;
        }

        vacuum_state.device_status = device_status;
        vacuum_state.actual_power = gripper_state.actual_power;
        vacuum_state.vacuum = gripper_state.vacuum;
        vacuum_state_publisher.publish(vacuum_state);
      }
    rate.sleep();
    }
  });

  read_thread.join();
  publish_thread.join();
  
  if (stop_at_shutdown) {
    gripper.stop();
  }

  return 0;
}
