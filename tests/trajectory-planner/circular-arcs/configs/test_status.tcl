# HAL config file to check vel/acc constraints
# NOTE: needs to be loaded before axis hal files
loadrt wcomp names=wcomp_xacc,wcomp_xvel,wcomp_yacc,wcomp_yvel,wcomp_zacc,wcomp_zvel,wcomp_aacc,wcomp_avel
loadrt minmax names=mm_xacc,mm_xvel,mm_yacc,mm_yvel,mm_zacc,mm_zvel,mm_aacc,mm_avel

addf wcomp_xacc servo-thread
addf wcomp_xvel servo-thread
addf wcomp_yacc servo-thread
addf wcomp_yvel servo-thread
addf wcomp_zacc servo-thread
addf wcomp_zvel servo-thread
addf wcomp_aacc servo-thread
addf wcomp_avel servo-thread

addf mm_xacc servo-thread
addf mm_xvel servo-thread
addf mm_yacc servo-thread
addf mm_yvel servo-thread
addf mm_zacc servo-thread
addf mm_zvel servo-thread
addf mm_aacc servo-thread
addf mm_avel servo-thread

net Xacc => wcomp_xacc.in mm_xacc.in
net Xvel => wcomp_xvel.in mm_xvel.in
net Yacc => wcomp_yacc.in mm_yacc.in
net Yvel => wcomp_yvel.in mm_yvel.in
net Zacc => wcomp_zacc.in mm_zacc.in
net Zvel => wcomp_zvel.in mm_zvel.in
net Aacc => wcomp_aacc.in mm_aacc.in
net Avel => wcomp_avel.in mm_avel.in

net acc-ok-x <= wcomp_xacc.out
net vel-ok-x <= wcomp_xvel.out
net acc-ok-y <= wcomp_yacc.out
net vel-ok-y <= wcomp_yvel.out
net acc-ok-z <= wcomp_zacc.out
net vel-ok-z <= wcomp_zvel.out
net acc-ok-a <= wcomp_aacc.out
net vel-ok-a <= wcomp_avel.out

loadrt match8 names=match_all

addf match_all servo-thread

# Connect match signals and default all to not loaded
# Enable the match-all pins in specific axis hal files
net acc-ok-x => match_all.a0
net vel-ok-x => match_all.a1
net acc-ok-y => match_all.a2
net vel-ok-y => match_all.a3
net acc-ok-z => match_all.a4
net vel-ok-z => match_all.a5
net acc-ok-a => match_all.a6
net vel-ok-a => match_all.a7

setp match_all.b0 0
setp match_all.b1 0
setp match_all.b2 0
setp match_all.b3 0
setp match_all.b4 0
setp match_all.b5 0
setp match_all.b6 0
setp match_all.b7 0

setp match_all.in 1
