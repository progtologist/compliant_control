#ifndef HARDWARE_INTERFACE_ADAPTER_H
#define HARDWARE_INTERFACE_ADAPTER_H

// ROS
#include <ros/node_handle.h>
#include <ros/time.h>

// hardware_interface
#include <hardware_interface/joint_command_interface.h>

// compliant control
#include <vigir_compliant_ros_controller/ConversionHelper.h>
#include <vigir_compliant_ros_controller/CustomTypes.h>
#include <vigir_compliant_ros_controller/CartForceController.h>
#include <vigir_compliant_ros_controller/CartVelController.h>
#include <vigir_compliant_ros_controller/InvKinController.h>

#include <joint_trajectory_controller/hardware_interface_adapter.h>

namespace compliant_controller {

template <class HardwareInterface, class State>
class HardwareInterfaceAdapter {
public:
  bool init(std::vector<std::string> segment_names, std::vector<typename HardwareInterface::ResourceHandleType>& joint_handles, ros::NodeHandle& controller_nh)
  {
    return false;
  }

  void starting(const ros::Time& time) {}
  void stopping(const ros::Time& time) {}

  void updateCommand(const ros::Time&     time,
                     const ros::Duration& period,
                     const State&         desired_state) {}
  Transform getTipPose() {return Transform();}
};

template <>
class HardwareInterfaceAdapter<hardware_interface::VelocityJointInterface, compliant_controller::CartState> {
public:
    bool init(std::vector<std::string> segment_names, std::vector<hardware_interface::JointHandle>& joint_handles, ros::NodeHandle& controller_nh) {
        joint_handles_ptr_ = &joint_handles;
        // resize pre-allocated variables
        velocities_.resize(joint_handles_ptr_->size());
        joint_velocities_.resize(joint_handles_ptr_->size());
        joint_positions_.resize(joint_handles_ptr_->size());

        // init controllers
        if (!cart_force_controller_.init(controller_nh, segment_names[0], segment_names[segment_names.size()-1], 10, 0, 0)) {
            return false;
        }
        if (!cart_vel_controller_.init(controller_nh, segment_names[0], segment_names[segment_names.size()-1])) {
            return false;
        }
        return true;
    }

    void starting(const ros::Time& time) {}
    void stopping(const ros::Time& time) {
        for (unsigned int i = 0; i < joint_handles_ptr_->size(); i++) {
            (*joint_handles_ptr_)[i].setCommand(0);
        }
    }

    void updateCommand(const ros::Time& time, const ros::Duration& period, const compliant_controller::CartState& desired_state) {
        updateJointState();
        // calculate correction vector in cartesian space
        compliant_controller::Vector6d twist;
        cart_force_controller_.calcCorrectionVector(desired_state.position, desired_state.velocity, twist, period.toSec());
        //ROS_INFO_STREAM("calculated twist: " << std::endl << twist);
        // transform cartesian vector to joint velocities
        cart_vel_controller_.update(twist, velocities_);

        // assign joint velocity command
        for (unsigned int i = 0; i < joint_handles_ptr_->size(); i++) {
            (*joint_handles_ptr_)[i].setCommand(velocities_(i));
        }
        //ROS_INFO_STREAM("joint velocity cmds: " << std::endl << velocities_);
    }

    // not realtime safe
    Transform getTipPose() {
        updateJointState();
        KDL::Frame pose;
        cart_force_controller_.getTipPose(pose);
        Transform transform;
        compliant_controller::ConversionHelper::kdlToEigen(pose, transform);
        return transform;
    }

private:
    void updateJointState() {
        // update joint state
        for (unsigned int i = 0; i < joint_positions_.size(); i++) {
            joint_positions_(i) = (*joint_handles_ptr_)[i].getPosition();
            joint_velocities_(i) = (*joint_handles_ptr_)[i].getVelocity();
        }
        cart_force_controller_.updateJointState(joint_positions_, joint_velocities_);
        cart_vel_controller_.updatePosition(joint_positions_);
    }

    std::vector<hardware_interface::JointHandle>* joint_handles_ptr_;
    compliant_controller::CartForceController cart_force_controller_;
    compliant_controller::CartVelController cart_vel_controller_;

    //pre-allocated variables
    compliant_controller::VectorNd velocities_;
    compliant_controller::VectorNd joint_positions_;
    compliant_controller::VectorNd joint_velocities_;
};

//template <>
//class HardwareInterfaceAdapter<hardware_interface::EffortJointInterface, compliant_controller::CartState>  {
//public:
//     bool init(std::vector<std::string> segment_names, std::vector<hardware_interface::JointHandle>& joint_handles, ros::NodeHandle& controller_nh) {
//        joint_handles_ptr_ = &joint_handles;
//        // resize pre-allocated variables
//        torques_.resize(joint_handles_ptr_->size());
//        joint_velocities_.resize(joint_handles_ptr_->size());
//        joint_positions_.resize(joint_handles_ptr_->size());
//        // init controller
//        if (!cart_force_controller_.init(controller_nh, segment_names[0], segment_names[segment_names.size()-1], 10, 0, 0)) {
//            return false;
//        }
//        return true;
//     }

//     void starting(const ros::Time& time) {}
//     void stopping(const ros::Time& time) {
//         for (unsigned int i = 0; i < joint_handles_ptr_->size(); i++) {
//             (*joint_handles_ptr_)[i].setCommand(0);
//         }
//     }

//     void updateCommand(const ros::Time& time, const ros::Duration& period, const compliant_controller::CartState& desired_state) {
//         // calculate correction force in cartesian space
//         compliant_controller::Vector6d force;
//         cart_force_controller_.calcCorrectionVector(desired_state.position, desired_state.velocity, force, period.toSec());

//         // map force to joint efforts
//         cart_force_controller_.calcTorques(force, torques_);

//         // assign effort cmd
//         for (unsigned int i = 0; i < joint_handles_ptr_->size(); i++) {
//             (*joint_handles_ptr_)[i].setCommand(torques_(i));
//         }

//     }

//     Transform getTipPose() {
//         updateJointState();
//         KDL::Frame pose;
//         cart_force_controller_.getTipPose(pose);
//         Transform transform;
//         compliant_controller::ConversionHelper::kdlToEigen(pose, transform);
//         return transform;
//     }
//private:
//     void updateJointState() {
//         // update joint state
//         for (unsigned int i = 0; i < joint_positions_.size(); i++) {
//             joint_positions_(i) = (*joint_handles_ptr_)[i].getPosition();
//             joint_velocities_(i) = (*joint_handles_ptr_)[i].getVelocity();
//         }
//         cart_force_controller_.updateJointState(joint_positions_, joint_velocities_);
//     }

//    std::vector<hardware_interface::JointHandle>* joint_handles_ptr_;
//    compliant_controller::CartForceController cart_force_controller_;

//    //pre-allocated variables
//    compliant_controller::VectorNd torques_;
//    compliant_controller::VectorNd joint_positions_;
//    compliant_controller::VectorNd joint_velocities_;
//};

template <>
class HardwareInterfaceAdapter<hardware_interface::PositionJointInterface, compliant_controller::CartState> {
public:
  bool init(std::vector<std::string> segment_names, std::vector<hardware_interface::JointHandle>& joint_handles, ros::NodeHandle& controller_nh)
  {
      // resize pre-allocated variables
      joint_handles_ptr_ = &joint_handles;
      joint_positions_.resize(joint_handles.size());
      joint_position_cmds_.resize(joint_handles.size());
      // init IK
      std::string moveit_group;
      if (!controller_nh.getParam("moveit_group", moveit_group)) {
        ROS_ERROR_STREAM("Couldn't find param 'moveit_group' in namespace " << controller_nh.getNamespace() << ".");
        return false;
      }

      if (!inv_kin_controller_.init(moveit_group)) { // add moveit group to config
          return false;
      }
    return true;
  }

  void starting(const ros::Time& time) {}
  void stopping(const ros::Time& time) {}

  void updateCommand(const ros::Time& time, const ros::Duration& period, const compliant_controller::CartState& desired_state) {
      for (unsigned int i = 0; i < joint_positions_.size(); i++) {
          joint_positions_(i) = (*joint_handles_ptr_)[i].getPosition();
      }
      inv_kin_controller_.updateJointState(joint_positions_);
      inv_kin_controller_.calcInvKin(desired_state.position, joint_position_cmds_);
      for (unsigned int i = 0; i < joint_position_cmds_.size(); i++) {
          (*joint_handles_ptr_)[i].setCommand(joint_position_cmds_(i));
      }
  }
  Transform getTipPose() {
      for (unsigned int i = 0; i < joint_positions_.size(); i++) {
          joint_positions_(i) = (*joint_handles_ptr_)[i].getPosition();
      }
      inv_kin_controller_.updateJointState(joint_positions_);

      Eigen::Affine3d pose;
      inv_kin_controller_.getTipTransform(pose);
      Transform transform;
      transform.rotation = pose.rotation();
      transform.translation = pose.translation();
      return transform;
  }

private:
  std::vector<hardware_interface::JointHandle>* joint_handles_ptr_;
  compliant_controller::InvKinController inv_kin_controller_;

  //pre-allocated variables
  compliant_controller::VectorNd joint_positions_;
  compliant_controller::VectorNd joint_position_cmds_;
};

template <>
class HardwareInterfaceAdapter<hardware_interface::EffortJointInterface, compliant_controller::CartState> {
public:
  bool init(std::vector<std::string> segment_names, std::vector<hardware_interface::JointHandle>& joint_handles, ros::NodeHandle& controller_nh)
  {
      // resize pre-allocated variables
      joint_handles_ptr_ = &joint_handles;
      joint_positions_.resize(joint_handles.size());
      joint_position_cmds_.resize(joint_handles.size());
      joint_cmds_ = compliant_controller::JointState(joint_handles.size());
      std::fill(joint_cmds_.velocity.begin(), joint_cmds_.velocity.end(),0);
      std::fill(joint_cmds_.acceleration.begin(), joint_cmds_.acceleration.end(),0);
      // init IK
      std::string moveit_group;
      if (!controller_nh.getParam("moveit_group", moveit_group)) {
        ROS_ERROR_STREAM("Couldn't find param 'moveit_group' in namespace " << controller_nh.getNamespace() << ".");
        return false;
      }

      if (!inv_kin_controller_.init(moveit_group)) { // add moveit group to config
          return false;
      }
      if (!jnt_pos_to_effort_hwi.init(joint_handles,controller_nh)) {
          return false;
      }
    return true;
  }

  void starting(const ros::Time& time) {
      jnt_pos_to_effort_hwi.starting(time);
  }
  void stopping(const ros::Time& time) {
      jnt_pos_to_effort_hwi.stopping(time);
  }

  void updateCommand(const ros::Time& time, const ros::Duration& period, const compliant_controller::CartState& desired_state) {
      for (unsigned int i = 0; i < joint_positions_.size(); i++) {
          joint_positions_(i) = (*joint_handles_ptr_)[i].getPosition();
      }
      inv_kin_controller_.updateJointState(joint_positions_);
      inv_kin_controller_.calcInvKin(desired_state.position, joint_position_cmds_);
      for (unsigned int i = 0; i < joint_position_cmds_.size(); i++) {
          //(*joint_handles_ptr_)[i].setCommand(joint_cmds_(i));
          joint_cmds_.position[i] = joint_position_cmds_(i);
          joint_error_.position[i] = joint_cmds_.position[i] - joint_positions_(i);
          joint_error_.velocity[i] = -(*joint_handles_ptr_)[i].getVelocity();
          joint_error_.acceleration[i] = 0;
      }
      jnt_pos_to_effort_hwi.updateCommand(time,period,joint_cmds_, joint_error_);

  }
  Transform getTipPose() {
      for (unsigned int i = 0; i < joint_positions_.size(); i++) {
          joint_positions_(i) = (*joint_handles_ptr_)[i].getPosition();
      }
      inv_kin_controller_.updateJointState(joint_positions_);

      Eigen::Affine3d pose;
      inv_kin_controller_.getTipTransform(pose);
      Transform transform;
      transform.rotation = pose.rotation();
      transform.translation = pose.translation();
      return transform;
  }

private:
  std::vector<hardware_interface::JointHandle>* joint_handles_ptr_;
  compliant_controller::InvKinController inv_kin_controller_;

  //pre-allocated variables
  compliant_controller::VectorNd joint_positions_;
  compliant_controller::VectorNd joint_position_cmds_;
  compliant_controller::JointState joint_cmds_;
  compliant_controller::JointState joint_error_;
  ::HardwareInterfaceAdapter<hardware_interface::EffortJointInterface, JointState> jnt_pos_to_effort_hwi;
};

}
#endif
