#include <ackermann_msgs/AckermannDriveStamped.h>
#include <ros/ros.h>
#include <sensor_msgs/LaserScan.h>
#include <std_msgs/Float32.h>
#include <std_msgs/Float64.h>
#include <ros/console.h>
#include <vector>
#include "math.h"

#define KP 0.36
#define KP_wall 1.15
#define KD 0.001
#define KI 0.005
#define SERVO_OFFSET 0.00
#define VEL_MAX 2.5
#define VEL_MIN 0.25
#define ANGLE_RANGE 270
#define DISIRED_DISTANCE_RIGHT 1
#define DISIRED_DISTANCE_LEFT 1
#define CAR_LENGTH 100
#define PI 3.1415927

class SubscribeAndPublish {
public:
    SubscribeAndPublish() {
        //Topic you want to publish
        pub_ = n_.advertise<ackermann_msgs::AckermannDriveStamped>("/vesc/low_level/ackermann_cmd_mux/input/teleop", 1);

        //Topic you want to subscribe
        sub_ = n_.subscribe("/scan", 1, &SubscribeAndPublish::callback, this);
    }

    void callback(const sensor_msgs::LaserScan& lidar_info) {
        //  Preprocess the LiDAR scan array. Expert implementation includes:
        //  1.Setting each value to the mean over some window
        //  2.Rejecting high values (eg. > 3m)
	ackermann_msgs::AckermannDriveStamped ackermann_drive_result;
        ranges = std::vector<double>(std::begin(lidar_info.ranges), std::end(lidar_info.ranges));
	float left_avg = ranges[900]; // initialize to left range
	float right_avg = ranges[360]; // initialize to right range
	float left_sum=0.0, right_sum=0.0;
	for(int i=0; i<60; i++) {
		left_sum += ranges[720+i]; // 15 deg sector from 45 to 60
		right_sum += ranges[300+i]; // 15 deg sector from -60 to -45
	}
	left_avg = left_sum/60;
	right_avg = right_sum/60;
	if (right_avg<=0.45 || left_avg<=0.45){
		ROS_INFO_STREAM(right_avg);
		ROS_INFO_STREAM(left_avg);
		ackermann_drive_result.drive.steering_angle = KP_wall*(left_avg-right_avg); // wall following
	}
	else {
		double min_angle = -90 / 180.0 * PI;
		unsigned int min_indx = (unsigned int)(std::floor((min_angle - lidar_info.angle_min) / lidar_info.angle_increment));
		double max_angle = 90 / 180.0 * PI;
		unsigned int max_indx = (unsigned int)(std::ceil((max_angle - lidar_info.angle_min) / lidar_info.angle_increment));
		for (unsigned int i = min_indx; i <= max_indx; i++) {
		    if (std::isinf(lidar_info.ranges[i]) || std::isnan(lidar_info.ranges[i])) {
		        ranges[i] = 0.0;
		    } else if (lidar_info.ranges[i] > lidar_info.range_max) {
		        ranges[i] = lidar_info.range_max;
		    }
		}
		
		//  Find closest point to LiDAR
		unsigned int closest_indx = min_indx;
		double closest_distance = lidar_info.range_max * 5;
		for (unsigned int i = min_indx; i <= max_indx; i++) {
		    double distance = ranges[i - 2] + ranges[i - 1] + ranges[i] + ranges[i + 1] + ranges[i + 2];
		    if (distance < closest_distance) {
		        closest_distance = distance;
		        closest_indx = i;
		    }
		}

		//  Eliminate all points inside 'bubble' (set them to zero) # Follow the gap method 06_07_23 --Ajinkya
		//unsigned int radius = 125;
		//for (unsigned int i = closest_indx - radius; i < closest_indx + radius + 1; i++) {
		    //ranges[i] = 0.0;
		//}
		// Disparity extender mathod (06_08_23) -- Ajinkya
		float r = lidar_info.ranges[closest_indx];
		float track_width = 0.23;
		float disparity_theta = (track_width/(r+1));
		int radius = std::ceil(disparity_theta/lidar_info.angle_increment);
		for (unsigned int i = closest_indx - radius; i < closest_indx + radius + 1; i++) {
		    ranges[i] = 0.0;
		} 

		//  Return the start index & end index of the max gap in free_space_ranges
		unsigned int start = min_indx;
		unsigned int end = min_indx;
		unsigned int current_start = min_indx - 1;
		unsigned int duration = 0;
		unsigned int longest_duration = 0;
		for (unsigned int i = min_indx; i <= max_indx; i++) {
		    // ROS_INFO_STREAM("angles " << lidar_info.angle_min + i * lidar_info.angle_increment << " ranges " << ranges[i]);
		    if (current_start < min_indx) {
		        if (ranges[i] > 0.0) {
		            current_start = i;
		        }
		    } else if (ranges[i] <= 0.0) {
		        duration = i - current_start;
		        if (duration > longest_duration) {
		            longest_duration = duration;
		            start = current_start;
		            end = i - 1;
		        }
		        //ROS_INFO_STREAM("duration " << duration);
		        current_start = min_indx - 1;
		    }
		}
		if (current_start >= min_indx) {
		    duration = max_indx + 1 - current_start;
		    if (duration > longest_duration) {
		        longest_duration = duration;
		        start = current_start;
		        end = max_indx;
		    }
		}

		//ROS_INFO_STREAM("angle_min " << lidar_info.angle_min);
		//ROS_INFO_STREAM(lidar_info.angle_increment);

		//  Start_i & end_i are start and end indicies of max-gap range, respectively
		//  Return index of best point in ranges
		//  Naive: Choose the furthest point within ranges and go there
		double current_max = 0.0;
		for (unsigned int i = start; i <= end; i++) {
		    if (ranges[i] > current_max) {
		        current_max = ranges[i];
		        angle = lidar_info.angle_min + i * lidar_info.angle_increment;
		    } else if (ranges[i] == current_max) {
		        if (std::abs(lidar_info.angle_min + i * lidar_info.angle_increment) < std::abs(angle)) {
		            angle = lidar_info.angle_min + i * lidar_info.angle_increment;
		        }
		    }
		}
		ackermann_drive_result.drive.steering_angle = angle;
        }
	
	ackermann_drive_result.drive.speed = std::min(VEL_MAX,(VEL_MIN+KP*(1/(std::abs(angle)+0.01))));
	pub_.publish(ackermann_drive_result);

       // SubscribeAndPublish::reactive_control();
    }

    void reactive_control() {
        ackermann_msgs::AckermannDriveStamped ackermann_drive_result;
        ackermann_drive_result.drive.steering_angle = angle;
	float left_dist = ranges[810] ;
	float right_dist = ranges[450];
	if (right_dist<=0.45 || left_dist<=0.45){
		ROS_INFO_STREAM(right_dist);
		ROS_INFO_STREAM(left_dist);
		ackermann_drive_result.drive.steering_angle = KP_wall*(left_dist-right_dist);
	}
	else {
	
	}

	ackermann_drive_result.drive.speed = std::min(VEL_MAX,(VEL_MIN+KP*(1/(std::abs(angle)+0.01))));

        /*if (std::abs(angle) > 20.0 / 180.0 * PI) {
            ackermann_drive_result.drive.speed = 0.6;
        } else if (std::abs(angle) > 10.0 / 180.0 * PI) {
            ackermann_drive_result.drive.speed = 1;
        } else if (std::abs(angle) > 10.0 / 180.0 * PI){
            ackermann_drive_result.drive.speed = 0.2;
        }*/
        pub_.publish(ackermann_drive_result);
        //ROS_INFO_STREAM(angle);
    }

private:
    ros::NodeHandle n_;
    ros::Publisher pub_;
    ros::Subscriber sub_;
    double angle;
    std::vector<double> ranges;

};  // End of class SubscribeAndPublish

int main(int argc, char** argv) {
    //Initiate ROS
    ros::init(argc, argv, "reactive_gap_follower");

    //Create an object of class SubscribeAndPublish that will take care of everything
    SubscribeAndPublish SAPObject;

    ros::spin();

    return 0;
}
