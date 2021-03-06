#include "../../include/auvsi16/basic_mission_function.hpp"
#include "../../include/auvsi16/auvsi16_general.hpp"
#include "../../include/auvsi16/robosub_control.hpp"
#include <std_msgs/String.h>
#include <std_msgs/Int16.h>
#include <auvsi16/ImageProcessing.h>
#include <auvsi16/RobosubLauncher.h>
#include "mavros_msgs/State.h"
#include "mavros_msgs/VFR_HUD.h"
#define	 MIN_GROUND_SPEED 0.2


void nodeSelectCB(const std_msgs::String& msg);
void imageProcessingCB				(const auvsi16::ImageProcessing& msg);
void stateCB									(const mavros_msgs::State& msg);
bool checkAUTOCruise					();

bool changeFlightModeDebug(string fm);
void checkGroundSpeed();
void vfrHUDCB									(const mavros_msgs::VFR_HUD& msg);

ros::Publisher pub_imgproc_select;

bool		imgproc_status = false;
int 		center_buoy_x	= 0;
int 		center_buoy_y = 0;
double	buoy_area			= 0;
double 	radius_buoy		= 0;
int 		buoy_number  	= 0;
double 		entrance_lat = 0;
double		entrance_long = 0;
double		ground_speed = 0;
double 		heading_to_deploy_auv = 0;

std_msgs::String node_status;
std_msgs::String node_feedback;
string		state;
int main(int argc, char **argv){

	ros::init(argc, argv, "interoperability_mission_simple");
	ros::NodeHandle nh("~");
	nh.getParam("heading_to_deploy_auv", heading_to_deploy_auv);
	nh.getParam("entrance_lat", entrance_lat);
	nh.getParam("entrance_long", entrance_long);
	image_transport::ImageTransport it(nh);
	setBMFConfiguration(nh);

	RobosubControl robosub_control;
	auvsi16::RobosubLauncher	robosub;
	
	changePID(0.3, 0, 0);

	
	ros::Subscriber 						sub_vfr_hud 		= nh.subscribe("/mavros/vfr_hud", 1, vfrHUDCB);
	ros::Subscriber 						sub_state 			= nh.subscribe("/mavros/state", 1, stateCB);
	

	ros::Publisher pub_robosub		= nh.advertise<auvsi16::RobosubLauncher>("/auvsi16/robosub/launcher/control", 16);
	ros::Publisher pub_run_status		= nh.advertise<std_msgs::String>("/auvsi16/mission/navigation/status", 16);
	ros::Publisher pub_node_select 	= nh.advertise<std_msgs::String>("/auvsi16/node/select", 16,true);
	ros::Subscriber sub_node_select = nh.subscribe("/auvsi16/node/select", 10, nodeSelectCB);

	ros::Subscriber sub_imgproc_data	= nh.subscribe("/auvsi16/node/image_processing/data", 10, imageProcessingCB);

	ROS_WARN_STREAM("Waiting for interoperability mission selected.");
	while (ros::ok() && node_status.data != "im:interoperability.start"){
		ros::spinOnce();
	}

	ROS_WARN_STREAM("Interoperability mission selected.");
	ROS_INFO_STREAM( "Heading to Deploy AUV\t\t: " << heading_to_deploy_auv);
	ROS_INFO_STREAM("Entrance Latitude\t\t: " << entrance_lat);
	ROS_INFO_STREAM("Entrance Longitude\t\t\t: " << entrance_long);
	
	// go to entrance point
	wp_sender->clearWaypointList();
	wp_sender->addWaypoint(entrance_lat, entrance_long);
	wp_sender->sendWaypointList();
	changeFlightModeDebug("AUTO");
	sleep(5);
	ros::spinOnce();
	ros::spinOnce();
	ros::spinOnce();
	ros::spinOnce();
	ros::spinOnce();
	while(checkAUTOCruise()) {
		checkGroundSpeed();
		ros::spinOnce();
	}
	
	sleep(5);
	// head to deploy AUV
	wp_sender->clearWaypointList();
	moveToHeading(20, heading_to_deploy_auv);
	wp_sender->sendWaypointList();
	changeFlightModeDebug("AUTO");
	sleep(5);
	ros::spinOnce();
	ros::spinOnce();
	ros::spinOnce();
	ros::spinOnce();
	ros::spinOnce();
	while(checkAUTOCruise()) {
		checkGroundSpeed();
		ros::spinOnce();
	}
	
	robosub_control.motorControl(IDLE_STATE, MAX_THROTTLE);
	// tells robosub to go full speed here
	
	ROS_WARN_STREAM("Rolling out robosub.");
	robosub.motor_direction = 0;
	robosub.motor_speed = 0;
	robosub.launcher_heater = true;
	pub_robosub.publish(robosub);
	sleep(12);
	robosub.motor_direction = 0;
	robosub.motor_speed = 0;
	robosub.launcher_heater = false;
	
	//wait another 12 seconds for auv to deploy
	sleep(12);
	robosub_control.motorControl(IDLE_STATE, IDLE_STATE);
	
	// wait until receive something here
	sleep(30);
	
	ROS_WARN_STREAM("Rolling in robosub.");
	robosub.motor_direction = 2;
	robosub.motor_speed = 70;
	robosub.launcher_heater = false;
	pub_robosub.publish(robosub);
	sleep(12);
	robosub.motor_direction = 0;
	robosub.motor_speed = 125;
	robosub.launcher_heater = false;
	pub_robosub.publish(robosub);
	
	node_feedback.data = "nc:interoperability.end";
	pub_node_select.publish(node_feedback);
	ros::shutdown();
	// if flightmode is HOLD, continue code

}

void nodeSelectCB(const std_msgs::String& msg){

	node_status.data = msg.data;
}


void imageProcessingCB(const auvsi16::ImageProcessing& msg){

	center_buoy_x	= msg.center_buoy_x;
	center_buoy_y = msg.center_buoy_y;
	buoy_area			= msg.buoy_area;
	radius_buoy		= msg.radius_buoy;
	buoy_number  	= msg.buoy_number;
	imgproc_status = msg.detection_status;
}

void 	vfrHUDCB	(const mavros_msgs::VFR_HUD& msg){

	ground_speed = msg.groundspeed;
}


void stateCB(const mavros_msgs::State& msg){
	state = msg.mode;
}

bool checkAUTOCruise(){

	if(state == "AUTO"){

		return true;
	}

	else{

		return false;
	}

}


void checkGroundSpeed(){
	if(ground_speed < MIN_GROUND_SPEED && checkAUTOCruise() ){
		changeFlightModeDebug("MANUAL");
		sleep(5);
		changeFlightModeDebug("AUTO");
		sleep(5);
	}
}

bool changeFlightModeDebug(string fm){
	if(changeFlightMode(fm.c_str())){
		ROS_WARN_STREAM("Changed to " << fm);
	}
	else {

		ROS_ERROR_STREAM("Failed changing to " << fm);
	}
}
