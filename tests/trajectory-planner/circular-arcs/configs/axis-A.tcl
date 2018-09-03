# create HAL signals for position commands from motion module
# loop position commands back to motion module feedback
net Apos axis.3.motor-pos-cmd => axis.3.motor-pos-fb ddt_a.in

# send the position commands thru differentiators to
# generate velocity and accel signals
net Avel ddt_a.out => ddt_av.in
net Aacc <= ddt_av.out

#Conservative limits
set acc_limit 1.0001
set vel_limit 1.01

setp wcomp_aacc.max $::AXIS_3(MAX_ACCELERATION)*$acc_limit
setp wcomp_aacc.min $::AXIS_3(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_avel.max $::AXIS_3(MAX_VELOCITY)*$vel_limit
setp wcomp_avel.min $::AXIS_3(MAX_VELOCITY)*-1.0*$vel_limit

# Enable match_all pins for A axis
setp match_all.b6 1
setp match_all.b7 1
