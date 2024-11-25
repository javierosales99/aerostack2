// Copyright 2024 Universidad Politécnica de Madrid
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright
//      notice, this list of conditions and the following disclaimer.
//
//    * Redistributions in binary form must reproduce the above copyright
//      notice, this list of conditions and the following disclaimer in the
//      documentation and/or other materials provided with the distribution.
//
//    * Neither the name of the Universidad Politécnica de Madrid nor the names of its
//      contributors may be used to endorse or promote products derived from
//      this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.


#include "swarm_behavior.hpp"

/** Auxiliar Functions **/
void generateDynamicPoint(
  const as2_msgs::msg::PoseWithID & msg,
  dynamic_traj_generator::DynamicWaypoint & dynamic_point)
{
  dynamic_point.setName(msg.id);
  Eigen::Vector3d position;
  position.x() = msg.pose.position.x;
  position.y() = msg.pose.position.y;
  position.z() = msg.pose.position.z;
  dynamic_point.resetWaypoint(position);
}

SwarmBehavior::SwarmBehavior()
: as2_behavior::BehaviorServer<as2_behavior_swarm_msgs::action::Swarm>("SwarmBehavior")
{
  RCLCPP_INFO(this->get_logger(), "SwarmBehavior constructor");
  // Centroid Pose
  new_centroid_ = std::make_shared<geometry_msgs::msg::PoseStamped>();
  centroid_.header.frame_id = "earth";
  centroid_.pose.position.x = 6;
  centroid_.pose.position.y = 0;
  centroid_.pose.position.z = 1.5;
  new_centroid_->header.frame_id = "earth";
  new_centroid_->pose.position.x = 6;
  new_centroid_->pose.position.y = 0;
  new_centroid_->pose.position.z = 1.5;
  swarm_tf_handler_ = std::make_shared<as2::tf::TfHandler>(this);
  broadcaster = std::make_unique<tf2_ros::TransformBroadcaster>(this);
  swarm_base_link_frame_id_ = as2::tf::generateTfName(this, "Swarm");
  transform.header.stamp = this->get_clock()->now();
  transform.header.frame_id = "earth";
  transform.child_frame_id = swarm_base_link_frame_id_;
  transform.transform.translation.x = centroid_.pose.position.x;
  transform.transform.translation.y = centroid_.pose.position.y;
  transform.transform.translation.z = centroid_.pose.position.z;
  broadcaster->sendTransform(transform);
  cbk_group_ = this->create_callback_group(
    rclcpp::CallbackGroupType::MutuallyExclusive);
  timer_ =
    this->create_wall_timer(
    std::chrono::microseconds(20),
    std::bind(&SwarmBehavior::timer_callback, this), cbk_group_);
  trajectory_generator_ = std::make_shared<dynamic_traj_generator::DynamicTrajectory>();
   init_drones(this->centroid_, this->drones_names_);
}

// Update Swarm Pose
void SwarmBehavior::update_pose(std::shared_ptr<const geometry_msgs::msg::PoseStamped> new_centroid, std::shared_ptr<geometry_msgs::msg::PoseStamped> & update_centroid){

}
// Updates dinamic Swam TF
void SwarmBehavior::timer_callback()
{
  transform.header.stamp = this->get_clock()->now();
  transform.transform.translation.x = new_centroid_.get()->pose.position.x;
  transform.transform.translation.x = new_centroid_.get()->pose.position.y;
  transform.transform.translation.x = new_centroid_.get()->pose.position.z;
  broadcaster->sendTransform(transform);
}

void SwarmBehavior::init_drones(
  geometry_msgs::msg::PoseStamped centroid,
  std::vector<std::string> drones_names_)
{
  std::vector<geometry_msgs::msg::Pose> poses;
  poses = two_drones(centroid_);

  for (auto drone_name : drones_names_) {
    std::shared_ptr<DroneSwarm> drone =
      std::make_shared<DroneSwarm>(this, drone_name, poses.front(), cbk_group_);
    drones_[drone_name] = drone;
    poses.erase(poses.begin());
    RCLCPP_INFO(
      this->get_logger(), "%s has the initial pose at x: %f, y: %f, z: %f", drones_.at(
        drone_name)->drone_id_.c_str(), drones_.at(
        drone_name)->init_pose_.position.x, drones_.at(
        drone_name)->init_pose_.position.y, drones_.at(
        drone_name)->init_pose_.position.z);
  }
}

bool SwarmBehavior::process_goal(
  std::shared_ptr<const as2_behavior_swarm_msgs::action::Swarm::Goal> goal,
  as2_behavior_swarm_msgs::action::Swarm::Goal & new_goal)
{
  RCLCPP_INFO(this->get_logger(), "Processing goal");
  // Check if the path is in the earth frame, if not convert it
  if (goal->header.frame_id == "") {
    RCLCPP_ERROR(this->get_logger(), "Path frame_id is empty");
    return false;
  }
  if (goal->path.size() == 0) {
    RCLCPP_ERROR(this->get_logger(), "Path is empty");
    return false;
  }
  if (goal->header.frame_id != "earth") {
    std::vector<as2_msgs::msg::PoseWithID> path_converted;
    path_converted.reserve(goal->path.size());

    geometry_msgs::msg::PoseStamped pose_msg;

    for (as2_msgs::msg::PoseWithID waypoint : goal->path) {
      pose_msg.pose = waypoint.pose;
      pose_msg.header = goal->header;
      if (!swarm_tf_handler_->tryConvert(pose_msg, "earth", tf_timeout)) {
        RCLCPP_ERROR(this->get_logger(), "SwarmBehavior: can not get waypoint in earth frame");
        return false;
      }
      waypoint.pose = pose_msg.pose;
      path_converted.push_back(waypoint);
    }
    new_goal.header.frame_id = "earth";
    new_goal.path = path_converted;
  }
  // Check if the swarm_yaw is in the earth frame if not convert it
  geometry_msgs::msg::QuaternionStamped q;
  q.header = goal->header;
  as2::frame::eulerToQuaternion(0.0f, 0.0f, new_goal.yaw_swarm.angle, q.quaternion);

  if (!swarm_tf_handler_->tryConvert(q, "earth", tf_timeout)) {
    RCLCPP_ERROR(this->get_logger(), "GoToBehavior: can not get target orientation in earth frame");
    return false;
  }

  new_goal.yaw_swarm.angle = as2::frame::getYawFromQuaternion(q.quaternion);
  return true;
}




bool SwarmBehavior::on_activate(
  std::shared_ptr<const as2_behavior_swarm_msgs::action::Swarm::Goal> goal)
{
  as2_behavior_swarm_msgs::action::Swarm::Goal new_goal = *goal;
  if (!process_goal(goal, new_goal)) {
    RCLCPP_ERROR(this->get_logger(), "SwarmBehavior: Error processing goal");
    return false;
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
  for (auto drone : drones_) {
    goal_future_handles_.push_back(drone.second->own_init());
  }
  dynamic_traj_generator::DynamicWaypoint::Vector waypoints_to_set;
  waypoints_to_set.reserve(goal->path.size() + 1);
  // PARA PROBAR
  std::vector<std::string> waypoint_ids;
  waypoint_ids.reserve(goal->path.size());
  if (goal->max_speed < 0) {
    RCLCPP_ERROR(this->get_logger(), "Goal max speed is negative");
    return false;
  }
  trajectory_generator_->setSpeed(goal->max_speed);
    for (as2_msgs::msg::PoseWithID waypoint : goal->path) {
    // Process each waypoint id
    if (waypoint.id == "") {
      RCLCPP_ERROR(this->get_logger(), "Waypoint ID is empty");
      return false;
    } else {
      // Else if waypoint ID is in the list of waypoint IDs, then return false
      if (std::find(
          waypoint_ids.begin(), waypoint_ids.end(),
          waypoint.id) != waypoint_ids.end())
      {
        RCLCPP_ERROR(
          this->get_logger(), "Waypoint ID %s is not unique",
          waypoint.id.c_str());
        return false;
      }
    }
        if (goal->header.frame_id != "earth") {
      geometry_msgs::msg::PoseStamped pose_stamped;
      pose_stamped.header = goal->header;
      pose_stamped.pose = waypoint.pose;

      waypoint.pose = pose_stamped.pose;
    }

    waypoint_ids.push_back(waypoint.id);
        dynamic_traj_generator::DynamicWaypoint dynamic_waypoint;
    generateDynamicPoint(waypoint, dynamic_waypoint);
    waypoints_to_set.emplace_back(dynamic_waypoint);
    }

  // Generate vector of waypoints for trajectory generator, from goal to
  // dynamic_traj_generator::DynamicWaypoint::Vector



  // trajectory_generator_->setSpeed(goal->max_speed);
//   for (as2_msgs::msg::PoseWithID waypoint : goal->path) {
//       // Set to dynamic trajectory generator
//     dynamic_traj_generator::DynamicWaypoint dynamic_waypoint;
//     generateDynamicPoint(waypoint, dynamic_waypoint);
//     waypoints_to_set.emplace_back(dynamic_waypoint);
//     RCLCPP_INFO(this->get_logger()," current %f",dynamic_waypoint.getCurrentPosition().x());
//     RCLCPP_INFO(this->get_logger()," origin %f",dynamic_waypoint.getOriginalPosition().x());

// }
  // Set waypoints to trajectory generator
  /* aqui al generador de trayectorias le estas pasando todos los puntos de paso de la trayetcoria que
   por la accion le has pedido que recorra*/ 
  trajectory_generator_->setWaypoints(waypoints_to_set);
  
  return true;
}
as2_behavior::ExecutionStatus SwarmBehavior::monitoring(
  const std::vector<std::shared_ptr<rclcpp_action::ClientGoalHandle<as2_msgs::action::FollowReference>>>
  goal_future_handles)
{
  as2_behavior::ExecutionStatus local_status;
  for (auto goal_handle : goal_future_handles) {
    switch (goal_handle->get_status()) {
      case rclcpp_action::GoalStatus::STATUS_EXECUTING:
        local_status = as2_behavior::ExecutionStatus::RUNNING;
        // printf("Running exe\n");
        break;
      case rclcpp_action::GoalStatus::STATUS_SUCCEEDED:
        local_status = as2_behavior::ExecutionStatus::RUNNING;
        // printf("Running suc\n");
        break;
      case rclcpp_action::GoalStatus::STATUS_ACCEPTED:
        local_status = as2_behavior::ExecutionStatus::RUNNING;
        // printf("Running acep\n");
        break;
      case rclcpp_action::GoalStatus::STATUS_ABORTED:
        local_status = as2_behavior::ExecutionStatus::FAILURE;
        // printf("Not running abort\n");
        break;
      case rclcpp_action::GoalStatus::STATUS_CANCELED:
        local_status = as2_behavior::ExecutionStatus::FAILURE;
        // printf("Not running cance\n");
        break;
      default:
        local_status = as2_behavior::ExecutionStatus::FAILURE;
        // printf("Not running def\n");
        break;
    }
  }
  return local_status;
}
as2_behavior::ExecutionStatus SwarmBehavior::on_run(
  const std::shared_ptr<const as2_behavior_swarm_msgs::action::Swarm::Goal> & goal,
  std::shared_ptr<as2_behavior_swarm_msgs::action::Swarm::Feedback> & feedback_msg,
  std::shared_ptr<as2_behavior_swarm_msgs::action::Swarm::Result> & result_msg)
{
  as2_behavior::ExecutionStatus local_status = monitoring(goal_future_handles_);
  if (local_status == as2_behavior::ExecutionStatus::FAILURE) {
    return as2_behavior::ExecutionStatus::FAILURE;
  }
  // evaluateTrajectory(eval_time_.seconds());
  // new_centroid_->pose.position.x = trajectory_command_.setpoints.begin()->position.x;
  // new_centroid_->pose.position.y = trajectory_command_.setpoints.begin()->position.y;
  // new_centroid_->pose.position.z = trajectory_command_.setpoints.begin()->position.z;
  return as2_behavior::ExecutionStatus::RUNNING;

}

void SwarmBehavior::setup(){
  trajectory_command_.header.frame_id = "earth";
  // le decimos en cuantos puntos queremos que divida nuestra trayectoria
  trajectory_command_.setpoints.resize(sampling_n_);

}

bool SwarmBehavior::evaluateTrajectory(
  double eval_time)
{
  as2_msgs::msg::TrajectoryPoint setpoint;
  dynamic_traj_generator::References traj_command;
  for (int i = 0; i < sampling_n_; i++) {
    bool succes_eval =
    trajectory_generator_->evaluateTrajectory(eval_time, traj_command);
    setpoint.position.x = traj_command.position.x();
    setpoint.position.y = traj_command.position.y();
    setpoint.position.z = traj_command.position.z();
    setpoint.twist.x = traj_command.velocity.x();
    setpoint.twist.y = traj_command.velocity.y();
    setpoint.twist.z = traj_command.velocity.z();
    setpoint.acceleration.x = traj_command.acceleration.x();
    setpoint.acceleration.y = traj_command.acceleration.y();
    setpoint.acceleration.z = traj_command.acceleration.z();
    trajectory_command_.setpoints[i] = setpoint;
    eval_time += sampling_dt_;
    if (!succes_eval) {
      return false;
    }
  }
  trajectory_command_.header.stamp = this->now();
  return true;
}

