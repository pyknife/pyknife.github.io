
// ros libs
#include "ros/ros.h"
#include "std_msgs/String.h"
#include <geometry_msgs/Twist.h>

// vrep libs
#include "vrep_common/VrepInfo.h"
#include "vrep_common/simRosStartSimulation.h"
#include "vrep_common/simRosStopSimulation.h"
#include "vrep_common/simRosEnablePublisher.h"
#include "vrep_common/simRosEnableSubscriber.h"
#include "vrep_common/simRosGetObjectHandle.h"
#include "vrep_common/JointSetStateData.h"
#include "../include/v_repConst.h"

bool simulationRunning=true;
float simulationTime=0.0f;
float speedMotorFront, speedMotorRear, speedMotorLeft, speedMotorRight = 0;

void infoCallback(const vrep_common::VrepInfo::ConstPtr& info)
{
  simulationTime=info->simulationTime.data;
  simulationRunning=(info->simulatorState.data&1)!=0;
}

void startStopSim(ros::NodeHandle n, int s){

  if (s == 0){
    ros::ServiceClient client_simStart = n.serviceClient<vrep_common::simRosStartSimulation>("/vrep/simRosStartSimulation");
    vrep_common::simRosStartSimulation srv_simStart;
    if(client_simStart.call(srv_simStart)){
      ROS_INFO("Simulation Started!"); // TODO check if service call response is correct
    }

  }

  else if (s == 1){
    ros::ServiceClient client_simStop = n.serviceClient<vrep_common::simRosStopSimulation>("/vrep/simRosStopSimulation");
    vrep_common::simRosStopSimulation srv_simStop;
    if(client_simStop.call(srv_simStop)){
      ROS_INFO("Simulation Stopped!"); // TODO check if service call response is correct
    }

  }

}




int *getMotorHandles(ros::NodeHandle n){

  int* motorHandlePtr = new int[4];

  ros::ServiceClient client_objectHandle = n.serviceClient<vrep_common::simRosGetObjectHandle>("/vrep/simRosGetObjectHandle");
  vrep_common::simRosGetObjectHandle srv_objectHandle;

  srv_objectHandle.request.objectName = "OmniWheel45_typeA";
  client_objectHandle.call(srv_objectHandle);
  motorHandlePtr[0] = srv_objectHandle.response.handle;

  srv_objectHandle.request.objectName = "OmniWheel45_typeB";
  client_objectHandle.call(srv_objectHandle);
  motorHandlePtr[1] = srv_objectHandle.response.handle;

  srv_objectHandle.request.objectName = "OmniWheel45_typeA#0";
  client_objectHandle.call(srv_objectHandle);
  motorHandlePtr[2] = srv_objectHandle.response.handle;

  srv_objectHandle.request.objectName = "OmniWheel45_typeB#0";
  client_objectHandle.call(srv_objectHandle);
  motorHandlePtr[3] = srv_objectHandle.response.handle;

  return motorHandlePtr;

}


void powerMotors(int* motorHandlePtr, ros::NodeHandle n, ros::Publisher motorSpeedPub, double speedMotorFront, double speedMotorRear, double speedMotorLeft, double speedMotorRight ){

  vrep_common::JointSetStateData motorSpeeds;



  motorSpeeds.handles.data.push_back(motorHandlePtr[0]);
  motorSpeeds.handles.data.push_back(motorHandlePtr[1]);
  motorSpeeds.handles.data.push_back(motorHandlePtr[2]);
  motorSpeeds.handles.data.push_back(motorHandlePtr[3]);
  motorSpeeds.setModes.data.push_back(2); // 2 is the speed mode
  motorSpeeds.setModes.data.push_back(2); // 2 is the speed mode
  motorSpeeds.setModes.data.push_back(2); // 2 is the speed mode
  motorSpeeds.setModes.data.push_back(2); // 2 is the speed mode
  motorSpeeds.values.data.push_back(speedMotorFront);
  motorSpeeds.values.data.push_back(speedMotorRear);
  motorSpeeds.values.data.push_back(speedMotorLeft);
  motorSpeeds.values.data.push_back(speedMotorRight);

  motorSpeedPub.publish(motorSpeeds);


}

void teleopKeyboardCb(const geometry_msgs::Twist::ConstPtr& vel){
    ROS_INFO("got a key stroke!");

  if(vel->linear.x != 0 && vel->angular.z == 0){
      speedMotorLeft = vel->linear.x * 10;
      speedMotorRight = -vel->linear.x * 10 ;
      speedMotorFront = 0;
      speedMotorRear = 0;
      ROS_INFO("go forward/backwars");

  }


  else if(vel->linear.x == 0 && vel->angular.z != 0){
      speedMotorFront = -vel->angular.z * 5 ;
      speedMotorRear = vel->angular.z * 5 ;
      speedMotorLeft = -vel->angular.z * 5 ;
      speedMotorRight = -vel->angular.z * 5 ;
      ROS_INFO("full left/right turn!");

  }

  else if(vel->linear.x == 0 && vel->angular.z == 0){
      speedMotorFront = 0;
      speedMotorRear = 0;
      speedMotorLeft = 0;
      speedMotorRight = 0;
      ROS_INFO("stop motors!");

  }




}



int main(int argc, char **argv)
{

  ros::init(argc, argv, "sim_start");
  ros::NodeHandle n;
  ros::Rate rate(100);
  std_msgs::String topicName;
  topicName.data = "omni";
  int* motorHandlePtr;

  ros::Subscriber subInfo=n.subscribe("/vrep/info", 1, infoCallback);
  startStopSim(n,0);
  getMotorHandles(n);
  ros::Subscriber subTeleopKey=n.subscribe("cmd_vel", 10, teleopKeyboardCb);

  ros::ServiceClient client_enableSubscriber=n.serviceClient<vrep_common::simRosEnableSubscriber>("/vrep/simRosEnableSubscriber");
  vrep_common::simRosEnableSubscriber srv_enableSubscriber;

  srv_enableSubscriber.request.topicName="/"+topicName.data+"/wheels"; // the topic name
  srv_enableSubscriber.request.queueSize=1; // the subscriber queue size (on V-REP side)
  srv_enableSubscriber.request.streamCmd=simros_strmcmd_set_joint_state; // the subscriber type

  if (client_enableSubscriber.call(srv_enableSubscriber)&&(srv_enableSubscriber.response.subscriberID!=-1)) {
    ros::Publisher motorSpeedPub=n.advertise<vrep_common::JointSetStateData>("omni/wheels",1);
      motorHandlePtr = getMotorHandles(n);
    while (ros::ok() && simulationRunning) {
      powerMotors(motorHandlePtr, n, motorSpeedPub, speedMotorFront, speedMotorRear, speedMotorLeft, speedMotorRight);

      ros::spinOnce();
      rate.sleep();
    }
    startStopSim(n,1); // ROS terminated, stop simulation
    ros::shutdown();
    return(0);

  }

}
