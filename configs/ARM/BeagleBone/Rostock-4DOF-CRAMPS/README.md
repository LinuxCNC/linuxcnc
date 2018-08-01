# GaOlSt CRAMPS configuration for Machinekit

3D printer configuration using Machinekits Python API for my Rostock Delta.

This configuration does not use the PRU pin hunting fix. If you want
enable this please take a look at the Fabrikator-Mini-CRAMPS
configuration.
This configuration is using python configuration of HAL, have modular design, support 4 axis for Delta,
using new Joint Angle Offsets and radius offsets to correct hardware build issues,
Configuration is using Velocity Extrusion mode because Axis A is considered as Rotational Axis.

To calibrate Delta use Dial indicator and following resource: http://www.escher3d.com/pages/wizards/wizarddelta.php


This config have 2 possible types of configuration
3DOF (default) - 3 degree of freedom (almost same as in configs/ARM/BeagleBone/MendelMax-CRAMPS/ with added lineardelta configuration)
4DOF - 4 degree of freedom (same as 3DOF + 4-th axis with required changes)

To use 4DOF configuration:
change config filename in "run.py" file from "CRAMPS.3DOF.ini" to "CRAMPS.4DOF.ini"

Joint offsets are configured in "CRAMPS.OTHER.inc" file under "[MACHINE]" section
new parameters:
JOINT_1_ANGLE_OFFSET - Degree of displacement from ideal JOINT position (positive or negative float value)
JOINT_1_RADIUS_OFFSET - JOINT Radius displacement from DELTA_R (positive or negative float value)
JOINT_2_ANGLE_OFFSET - Degree of displacement from ideal JOINT position (positive or negative float value)
JOINT_2_RADIUS_OFFSET - JOINT Radius displacement from DELTA_R (positive or negative float value)

If parameter is missing in config - then it considered as "0.00"

JOINT Angles are considered following way:
if you look at XY plane of LinearDelta printer then X0-Y0 is the center of print area
Line from center to X"DELTA_R"-Y0 points to 0 degree
Line from center to X0-Y"DELTA_R" points to 90 degree - JOINT_0
JOINT_1 is displaced at 120 degree from JOINT_0 - (90+120) = 210
JOINT_2 is displaced at 120 degree from JOINT_1 - (210+120) = 330
and closing the loop
JOINT_0 is displaced at 120 degree from JOINT_2 - (330+120)= 450 = (360+90) = 90

in 4DOF configuration we have additional JOINT_3 (Rotational Axis - A)
it's considered as rotational axis on Y axis
Center of rotation going from Y"+DELTA_R",X0,Z0 to Y"-DELTA_R",X0,Z0
Physicaly it`s looking like rotational device parallel to build plate with center of rotation going from 90 degree to (90+180) = 270 degree.

4DOF configuration is considered as experimental - currently no slicer exist which can support 4-th axis, but you can use imagination.
