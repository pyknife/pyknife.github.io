<html>
<body>
<h1>Autonomous-Navigation-with-SLAM</h1>
<p>
Autonomous Navigation of a Pioneer_p3dx in Vrep using frontier_exploration, gmapping, move_base. 
Folders contain parameter files, launch files, Rviz files. 

The launch file among many others which works is ex.launch
After cloning the repository in your catkin workspace, open a terminal and run
</p>
<p>
<ul style="list-style-type:none">
 <li>$ catkin_make </li>
 <li>$ source ./devel/setup.bash </li>
 <li>$ roslaunch (project_name) ex.launch </li>
</ul>
</p>
<p>
<h3>NOTE:</h3> If RVIZ crashes, close all other terminals, except those that are running roscore and vrep, and then retry after sourcing setup.bash file.
</p>
<p>
<h3> NOTE:</h3> As (project_name) you can rename it yourself from CMakeLists.txt and package.xml.
</p>

<h2>Problems:</h2>
<p>
<ol>
 <li>After launching the program, it starts registering Laser Scans and Pose but soon fails to acquire that data.</li>
 <li>After publishing exploration boundaries and an initial point the robot doesn't move.</li>
 <li>CostMap2DT Transform data aquisition fails due to a timeout.</li>
 <li>In V-Rep terminal an ERROR message is displayed saying /explore_server requires base_scan to have datatype sensor_msgs/PointCloud but out version has sensor_msgs/LaserScan. Dropping the connection! </li>
 </ol>
 </p>
 <p>
 Help will be much appreaciated in figuring out how to make the algorithm work.
 Thank you.
 </p>
</body>
</html>
