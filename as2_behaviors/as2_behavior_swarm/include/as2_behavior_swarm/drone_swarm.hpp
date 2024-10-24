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

#ifndef DRONE_SWARM_HPP_
#define DRONE_SWARM_HPP_

#include <memory>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include "geometry_msgs/msg/pose_stamped.hpp"
#include "as2_core/names/topics.hpp"
#include "as2_core/utils/tf_utils.hpp"
#include "as2_msgs/msg/platform_info.hpp"
#include "as2_motion_reference_handlers/position_motion.hpp"

class DroneSwarm
{
public:
  DroneSwarm(as2::Node * node_ptr);
  ~DroneSwarm() {}

private:
  as2::Node * node_ptr_;

  void drone_pose_callback(const geometry_msgs::msg::PoseStamped::SharedPtr _pose_msg);
  void platform_info_callback(const as2_msgs::msg::PlatformInfo::SharedPtr _platform_info_msg);
  std::shared_ptr<as2::tf::TfHandler> tf_handler_;
  std::string base_link_frame_id_;
  rclcpp::Subscription<as2_msgs::msg::PlatformInfo>::SharedPtr platform_info_sub_;
  rclcpp::Subscription<geometry_msgs::msg::PoseStamped>::SharedPtr drone_pose_sub_;
  std::shared_ptr<as2::motionReferenceHandlers::PositionMotion> position_motion_handler_ = nullptr;

public:
  geometry_msgs::msg::PoseStamped drone_pose_;
  as2_msgs::msg::PlatformInfo platform_info_;
  std::string drone_name_;
  // quiero acceder desde la clase swarm
  void ownInit();


};


#endif // DRONE_SWARM_HPP_
