# HAL config file to check vel/acc constraints
#
loadrt wcomp names=wcomp_xacc,wcomp_xvel,wcomp_yacc,wcomp_yvel,wcomp_zacc,wcomp_zvel,wcomp_uacc,wcomp_uvel,wcomp_vacc,wcomp_vvel,wcomp_wacc,wcomp_wvel
loadrt minmax names=mm_xacc,mm_xvel,mm_yacc,mm_yvel,mm_zacc,mm_zvel,mm_aacc,mm_avel,mm_uacc,mm_uvel,mm_vacc,mm_vvel,mm_wacc,mm_wvel

loadrt flipflop names=samplehold
addf wcomp_xacc servo-thread
addf wcomp_xvel servo-thread
addf wcomp_yacc servo-thread
addf wcomp_yvel servo-thread
addf wcomp_zacc servo-thread
addf wcomp_zvel servo-thread
addf wcomp_uacc servo-thread
addf wcomp_uvel servo-thread
addf wcomp_vacc servo-thread
addf wcomp_vvel servo-thread
addf wcomp_wacc servo-thread
addf wcomp_wvel servo-thread

addf mm_xacc servo-thread
addf mm_xvel servo-thread
addf mm_yacc servo-thread
addf mm_yvel servo-thread
addf mm_zacc servo-thread
addf mm_zvel servo-thread
addf mm_uacc servo-thread
addf mm_uvel servo-thread
addf mm_vacc servo-thread
addf mm_vvel servo-thread
addf mm_wacc servo-thread
addf mm_wvel servo-thread

net Xacc => wcomp_xacc.in mm_xacc.in
net Xvel => wcomp_xvel.in mm_xvel.in
net Yacc => wcomp_yacc.in mm_yacc.in
net Yvel => wcomp_yvel.in mm_yvel.in
net Zacc => wcomp_zacc.in mm_zacc.in
net Zvel => wcomp_zvel.in mm_zvel.in
net Uacc => wcomp_uacc.in mm_uacc.in
net Uvel => wcomp_uvel.in mm_uvel.in
net Vacc => wcomp_vacc.in mm_vacc.in
net Vvel => wcomp_vvel.in mm_vvel.in
net Wacc => wcomp_wacc.in mm_wacc.in
net Wvel => wcomp_wvel.in mm_wvel.in

net acc-ok-x <= wcomp_xacc.out
net vel-ok-x <= wcomp_xvel.out
net acc-ok-y <= wcomp_yacc.out
net vel-ok-y <= wcomp_yvel.out
net acc-ok-z <= wcomp_zacc.out
net vel-ok-z <= wcomp_zvel.out
net acc-ok-u <= wcomp_uacc.out
net vel-ok-u <= wcomp_uvel.out
net acc-ok-v <= wcomp_vacc.out
net vel-ok-v <= wcomp_vvel.out
net acc-ok-w <= wcomp_wacc.out
net vel-ok-w <= wcomp_wvel.out

set acc_limit 1.0001
set vel_limit 1.01

setp wcomp_xacc.max $::AXIS_0(MAX_ACCELERATION)*$acc_limit
setp wcomp_xacc.min $::AXIS_0(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_xvel.max $::AXIS_0(MAX_VELOCITY)*$vel_limit
setp wcomp_xvel.min $::AXIS_0(MAX_VELOCITY)*-1.0*$vel_limit

setp wcomp_yacc.max $::AXIS_1(MAX_ACCELERATION)*$acc_limit
setp wcomp_yacc.min $::AXIS_1(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_yvel.max $::AXIS_1(MAX_VELOCITY)*$vel_limit
setp wcomp_yvel.min $::AXIS_1(MAX_VELOCITY)*-1.0*$vel_limit

setp wcomp_zacc.max $::AXIS_2(MAX_ACCELERATION)*$acc_limit
setp wcomp_zacc.min $::AXIS_2(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_zvel.max $::AXIS_2(MAX_VELOCITY)*$vel_limit
setp wcomp_zvel.min $::AXIS_2(MAX_VELOCITY)*-1.0*$vel_limit

setp wcomp_uacc.max $::AXIS_6(MAX_ACCELERATION)*$acc_limit
setp wcomp_uacc.min $::AXIS_6(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_uvel.max $::AXIS_6(MAX_VELOCITY)*$vel_limit
setp wcomp_uvel.min $::AXIS_6(MAX_VELOCITY)*-1.0*$vel_limit

setp wcomp_vacc.max $::AXIS_7(MAX_ACCELERATION)*$acc_limit
setp wcomp_vacc.min $::AXIS_7(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_vvel.max $::AXIS_7(MAX_VELOCITY)*$vel_limit
setp wcomp_vvel.min $::AXIS_7(MAX_VELOCITY)*-1.0*$vel_limit

setp wcomp_wacc.max $::AXIS_8(MAX_ACCELERATION)*$acc_limit
setp wcomp_wacc.min $::AXIS_8(MAX_ACCELERATION)*-1.0*$acc_limit
setp wcomp_wvel.max $::AXIS_8(MAX_VELOCITY)*$vel_limit
setp wcomp_wvel.min $::AXIS_8(MAX_VELOCITY)*-1.0*$vel_limit

loadrt match8 names=match_all

addf match_all servo-thread

net acc-ok-x => match_all.a0
setp match_all.b0 1
net vel-ok-x => match_all.a1
setp match_all.b1 1
net acc-ok-y => match_all.a2
setp match_all.b2 1
net vel-ok-y => match_all.a3
setp match_all.b3 1
net acc-ok-z => match_all.a4
setp match_all.b4 1
net vel-ok-z => match_all.a5
setp match_all.b5 1

setp match_all.a6 0
setp match_all.a7 0
setp match_all.b6 0
setp match_all.b7 0

setp match_all.in 1

net constraints-ok <= match_all.out
