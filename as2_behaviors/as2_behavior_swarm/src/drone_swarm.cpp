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

#include "drone_swarm.hpp"


DroneSwarm::DroneSwarm(
  as2::Node * node_ptr, std::string drone_id,
  geometry_msgs::msg::Pose init_pose, rclcpp::CallbackGroup::SharedPtr cbk_group)
: node_ptr_(node_ptr)
{
  RCLCPP_INFO(node_ptr_->get_logger(), "Init %s", drone_id.c_str());
  drone_id_ = drone_id;
  init_pose_ = init_pose;
  cbk_group_ = cbk_group;

  drone_pose_sub_ = node_ptr_->create_subscription<geometry_msgs::msg::PoseStamped>(
    drone_id_ + "/" + as2_names::topics::self_localization::pose,
    as2_names::topics::self_localization::qos,
    std::bind(&DroneSwarm::drone_pose_callback, this, std::placeholders::_1));

  // Static tf (Swarm --> drone_ref)
  base_link_frame_id_ = as2::tf::generateTfName(
    node_ptr, drone_id_ + "_ref");
  parent_frame_id = as2::tf::generateTfName(node_ptr, "Swarm");
  tfstatic_broadcaster_ = std::make_shared<tf2_ros::StaticTransformBroadcaster>(node_ptr);
  transform.header.stamp = node_ptr->get_clock()->now();
  transform.header.frame_id = parent_frame_id;
  transform.child_frame_id = base_link_frame_id_;
  transform.transform.translation.x = init_pose.position.x;
  transform.transform.translation.y = init_pose.position.y;
  transform.transform.translation.z = init_pose.position.z;
  tfstatic_broadcaster_->sendTransform(transform);

  // FollowReference
  follow_reference_client_ = rclcpp_action::create_client<as2_msgs::action::FollowReference>(
    node_ptr_,
    "/" + drone_id_ + "/" + as2_names::actions::behaviors::followreference, cbk_group_);
}

std::shared_ptr<rclcpp_action::ClientGoalHandle<as2_msgs::action::FollowReference>>
DroneSwarm::own_init()
{
  RCLCPP_INFO(node_ptr_->get_logger(), "Init %s FollowReference", drone_id_.c_str());
  if (!follow_reference_client_->wait_for_action_server(
      std::chrono::seconds(5)))
  {
    RCLCPP_ERROR(
      node_ptr_->get_logger(),
      "Follow Reference Action server not available after waiting.");
    return nullptr;
  }

  // Reference to follow
  auto goal_reference_msg = as2_msgs::action::FollowReference::Goal();
  goal_reference_msg.target_pose.header.frame_id = "Swarm/" + drone_id_ + "_ref";
  goal_reference_msg.target_pose.header.stamp = node_ptr_->get_clock()->now();
  goal_reference_msg.target_pose.point.x = 0;
  goal_reference_msg.target_pose.point.y = 0;
  goal_reference_msg.target_pose.point.z = 0;
  goal_reference_msg.yaw.mode = as2_msgs::msg::YawMode::KEEP_YAW;
  goal_reference_msg.max_speed_x = 0.5;
  goal_reference_msg.max_speed_y = 0.5;
  goal_reference_msg.max_speed_z = 0.5;

  // Handle future
  auto goal_handle_future_follow_reference = follow_reference_client_->async_send_goal(
    goal_reference_msg);
  auto goal_handle_follow_reference = goal_handle_future_follow_reference.get();
  return goal_handle_follow_reference;
}


void DroneSwarm::drone_pose_callback(const geometry_msgs::msg::PoseStamped::SharedPtr _pose_msg)
{
  drone_pose_ = *_pose_msg;
}
