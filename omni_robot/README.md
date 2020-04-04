## Omni-wheel Robot Simulation

Note: This is an ongoing project. For now only keyboard teleop and mapping is available.

#### 1. Folder Organization

* ros: source/include/launch files
* sim: VREP scene

#### 2. VREP

Run VREP and open scene from sim/vrep folder

#### 3. ROS

Launch files:

```
roslaunch omni_robot teleop_key.launch
roslaunch omni_robot mapping.launch
```
Note: To able launch ROS nodes you need to install [teleop_twist_keyboard](http://wiki.ros.org/teleop_twist_keyboard) package by running this command:

`sudo apt-get install ros-indigo-teleop-twist-keyboard`
