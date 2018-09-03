# create HAL signals for position commands from motion module
# loop position commands back to motion module feedback
net Zpos axis.2.motor-pos-cmd => axis.2.motor-pos-fb ddt_z.in

# send the position commands thru differentiators to
# generate velocity and accel signals
net Zvel ddt_z.out => ddt_zv.in vel_xyz.in0
net Zacc <= ddt_zv.out

#Conservative limits
set acc_limit 1.0001
set vel_limit 1.01

setp wcomp_zacc.max $::AXIS_2(MAX_ACCELERATION)*$acc_limit
setp wcomp_zacc.min $::AXIS_2(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_zvel.max $::AXIS_2(MAX_VELOCITY)*$vel_limit
setp wcomp_zvel.min $::AXIS_2(MAX_VELOCITY)*-1.0*$vel_limit

# Enable match_all pins for Z axis
setp match_all.b4 1
setp match_all.b5 1

