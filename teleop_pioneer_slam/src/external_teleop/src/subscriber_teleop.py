#!/usr/bin/env python
import math
import rospy
import tf2_ros
import tf2_msgs.msg
import geometry_msgs.msg
from geometry_msgs.msg import Twist, Pose, Quaternion
from sensor_msgs.msg import LaserScan
from std_msgs.msg import String

import tf
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Point, Vector3


class Node(object):
    """docstring for Node."""
    def __init__(self):
        super(Node, self).__init__()
        self.odom_last_pos = None
        self.cmd_vel = None

    def create_odom(self, pos, quat_msg):
        if self.odom_last_pos is not None:
            current_time = rospy.Time.now()
            odom_pub = rospy.Publisher("/odom", Odometry, queue_size=50)
            odom_broadcaster = tf.TransformBroadcaster()

            delta_x = pos.position.x - self.odom_last_pos.position.x
            delta_y = pos.position.x - self.odom_last_pos.position.x
            delta_z = pos.position.x - self.odom_last_pos.position.x


            odom_broadcaster.sendTransform(
                (delta_x, delta_y, delta_z),
                (quat_msg.x, quat_msg.y, quat_msg.z, quat_msg.w),
                current_time,
                "base",
                "hokuyo"
            )

            # next, we'll publish the odometry message over ROS
            odom = Odometry()
            odom.header.stamp = current_time
            odom.header.frame_id = "base"

            # set the position
            odom.pose.pose = Pose(Point(delta_x, delta_y, delta_z), quat_msg)

            # set the velocity
            odom.child_frame_id = "hokuyo"
            odom.twist.twist = self.cmd_vel

            # publish the message
            odom_pub.publish(odom)
            rospy.loginfo("Published odom")

        else:
            rospy.loginfo("Not Published odom")
        self.odom_last_pos = pos


    def pose_callback(self, data):
        rospy.loginfo(rospy.get_caller_id() + "Received from /pioneer_pose")
        # rospy.loginfo(rospy.get_caller_id() + "I heard %s", data)
        self.talker_pose(data)

    def cmd_vel_callback(self, data):
        global cmd_vel
        rospy.loginfo(rospy.get_caller_id() + "Received from /joy_teleop/cmd_vel")
        # rospy.loginfo(rospy.get_caller_id() + "I heard %s", data)
        cmd_vel = data
        self.talker_pioneer_cmd_vel(data)

    def laser_scan_callback(self, data):
        rospy.loginfo(rospy.get_caller_id() + "Received from /hokuyo_scan")
        # rospy.loginfo(rospy.get_caller_id() + "I heard %s", data)
        data.intensities = len(data.ranges)*[200]
        self.talker_extern_scan(data)

    def talker_pose(self, data):
        # pub = rospy.Publisher('/pioneer_cmd_vel', Twist, queue_size=10)
        rospy.loginfo(rospy.get_caller_id() + "Sent to /tf")
        pub = rospy.Publisher('/tf_laser', Pose)
        # rospy.loginfo(data)
        pub.publish(data)

        pub_tf = rospy.Publisher("/tf_static", tf2_msgs.msg.TFMessage, queue_size=1)
        t = geometry_msgs.msg.TransformStamped()
        t.header.frame_id = "base"
        t.header.stamp = rospy.Time.now()
        t.child_frame_id = "hokuyo"
        t.transform.translation.x = data.position.x
        t.transform.translation.y = data.position.y
        t.transform.translation.z = data.position.z

        pos = data

        quat_tf = [data.orientation.x, data.orientation.y, data.orientation.z, data.orientation.w]
        quatNorm = math.sqrt(quat_tf[0] * quat_tf[0] + quat_tf[1] *
                             quat_tf[1] + quat_tf[2] * quat_tf[2] +
                             quat_tf[3] * quat_tf[3])

        quat_msg = Quaternion(quat_tf[0]/quatNorm, quat_tf[1]/quatNorm, quat_tf[2]/quatNorm, quat_tf[3]/quatNorm)

        t.transform.rotation.x = quat_msg.x
        t.transform.rotation.y = quat_msg.y
        t.transform.rotation.z = quat_msg.z
        t.transform.rotation.w = quat_msg.w

        tfm = tf2_msgs.msg.TFMessage([t])
        pub_tf.publish(tfm)

        self.create_odom(pos, quat_msg)

    def talker_pioneer_cmd_vel(self, data):
        # pub = rospy.Publisher('/pioneer_cmd_vel', Twist, queue_size=10)
        rospy.loginfo(rospy.get_caller_id() + "Sent to /pioneer_cmd_vel")
        pub = rospy.Publisher('/pioneer_cmd_vel', Twist)
        # rospy.loginfo(data)
        self.cmd_vel = data
        pub.publish(data)

    def talker_extern_scan(self, data):
        # pub = rospy.Publisher('/pioneer_cmd_vel', Twist, queue_size=10)
        rospy.loginfo(rospy.get_caller_id() + "Sent to /extern_scan")

        pub = rospy.Publisher('/extern_scan', LaserScan)
        data.header.frame_id = "base"
        # rospy.loginfo(data)
        pub.publish(data)

    def external_listener(self):

        # In ROS, nodes are uniquely named. If two nodes with the same
        # name are launched, the previous one is kicked off. The
        # anonymous=True flag means that rospy will choose a unique
        # name for our 'listener' node so that multiple listeners can
        # run simultaneously.
        rospy.init_node('external_listener', anonymous=True)

        rospy.Subscriber('/joy_teleop/cmd_vel', Twist, self.cmd_vel_callback)
        rospy.Subscriber('/hokuyo_scan', LaserScan, self.laser_scan_callback)
        rospy.Subscriber('/pioneer_pose', Pose, self.pose_callback)


        # spin() simply keeps python from exiting until this node is stopped
        rospy.spin()

if __name__ == '__main__':
    node = Node()
    node.external_listener()
