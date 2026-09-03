#!/bin/bash
set -e

${SUDO} halcompile --install jaccheck.c >/dev/null

# One hal file per module: they all define the same entry points, so
# only one can be loaded at a time.  A run that leaves the sweep at its
# default takes each rotary through the quarter and half turns where a
# sine changes sign or a cosine vanishes; the arms and the parallel
# machines name their own, away from the poses they cannot hold.
# ONLY=<module> in the environment runs the entries for that module alone
run() {
    local hal
    case "$1" in "${ONLY:-}"*) ;; *) return 0 ;; esac
    hal=$(mktemp --suffix=.hal)
    { printf 'loadrt %s\n' "$1"
      printf '%s\n' "$2"
      printf 'loadrt jaccheck %s\n' "$3"
    } > "$hal"
    echo "=== $1"
    halrun -f "$hal"
    rm -f "$hal"
}

# identity, including a gantry: two joints on one letter is the case where
# the forward is not one to one, so it is checked against the inverse
run "trivkins coordinates=XYZ" "" "joints=3"
run "trivkins coordinates=XYZY kinstype=BOTH" "" "joints=4 check=inv"
run "trivkins coordinates=XYZABCUVW" "" "joints=9 r1=3 r2=5"
run "userkins" "" "joints=3"
run "millturn" "" "joints=4"

# linear maps and one rotation
run "corexykins" "" "joints=9"
run "rotatekins" "" "joints=9 r1=5"
run "matrixkins" \
    "setp matrixkins.C_xy 0.02
setp matrixkins.C_xz -0.01
setp matrixkins.C_yx 0.03
setp matrixkins.C_yz 0.015
setp matrixkins.C_zx -0.02
setp matrixkins.C_zy 0.01
setp matrixkins.C_zz 1.001" \
    "joints=9"

# tables and heads; offsets set so no term drops out.  The forward and
# inverse of maxkins do not agree away from c = 0 and u = 0, which a
# separate fix addresses; until then its Jacobian, the derivative of the
# inverse, is checked against the inverse alone.
run "maxkins" \
    "setp maxkins.pivot-length 100" \
    "joints=9 r1=4 r2=5 base=10,20,30,0,0,0,7,0,3 check=inv"

run "5axiskins coordinates=XYZBCW" "" "joints=6 r1=3 r2=4 base=10,20,30,0,0,5"
run "5axiskins coordinates=XYZBCW sparm=identityfirst" "" "joints=6 r1=3 r2=4 base=10,20,30,0,0,5"

run "xyzac-trt-kins coordinates=XYZAC" \
    "setp xyzac-trt-kins.y-offset 3
setp xyzac-trt-kins.z-offset 11
setp xyzac-trt-kins.tool-offset 7
setp xyzac-trt-kins.x-rot-point 1
setp xyzac-trt-kins.y-rot-point 2
setp xyzac-trt-kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4"

run "xyzbc-trt-kins coordinates=XYZBC" \
    "setp xyzbc-trt-kins.x-offset 3
setp xyzbc-trt-kins.z-offset 11
setp xyzbc-trt-kins.tool-offset 7
setp xyzbc-trt-kins.x-rot-point 1
setp xyzbc-trt-kins.y-rot-point 2
setp xyzbc-trt-kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4"

# and both with the rotation sense the chapter asks for
run "xyzac-trt-kins coordinates=XYZAC" \
    "setp xyzac-trt-kins.conventional-directions 1
setp xyzac-trt-kins.y-offset 3
setp xyzac-trt-kins.z-offset 11
setp xyzac-trt-kins.tool-offset 7
setp xyzac-trt-kins.x-rot-point 1
setp xyzac-trt-kins.y-rot-point 2
setp xyzac-trt-kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4"

run "xyzbc-trt-kins coordinates=XYZBC" \
    "setp xyzbc-trt-kins.conventional-directions 1
setp xyzbc-trt-kins.x-offset 3
setp xyzbc-trt-kins.z-offset 11
setp xyzbc-trt-kins.tool-offset 7
setp xyzbc-trt-kins.x-rot-point 1
setp xyzbc-trt-kins.y-rot-point 2
setp xyzbc-trt-kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4"

run "xyzab_tdr_kins" \
    "setp xyzab_tdr_kins.x-offset 3
setp xyzab_tdr_kins.z-offset 11
setp xyzab_tdr_kins.tool-offset-z 7
setp xyzab_tdr_kins.x-rot-point 1
setp xyzab_tdr_kins.y-rot-point 2
setp xyzab_tdr_kins.z-rot-point 5" \
    "joints=5 r1=3 r2=4"

# The nutating heads read their rotary angles from the joint argument of
# the inverse rather than from the pose, so differencing the inverse
# about a pose cannot see the coupling; the forward is the check here.
run "xyzacb_trsrn" \
    "setp xyzacb_trsrn_kins.nut-angle 45
setp xyzacb_trsrn_kins.y-pivot 100
setp xyzacb_trsrn_kins.z-pivot 200
setp xyzacb_trsrn_kins.x-offset 5
setp xyzacb_trsrn_kins.y-offset 7
setp xyzacb_trsrn_kins.y-rot-axis 300
setp xyzacb_trsrn_kins.z-rot-axis 400
setp xyzacb_trsrn_kins.tool-offset-z 50
setp xyzacb_trsrn_kins.pre-rot 0.3
setp xyzacb_trsrn_kins.primary-angle 20
setp xyzacb_trsrn_kins.secondary-angle 35" \
    "joints=6 r1=3 r2=4 r3=5 check=fwd"

run "xyzbca_trsrn" \
    "setp xyzbca_trsrn_kins.nut-angle 45
setp xyzbca_trsrn_kins.x-pivot 100
setp xyzbca_trsrn_kins.z-pivot 200
setp xyzbca_trsrn_kins.x-offset 5
setp xyzbca_trsrn_kins.y-offset 7
setp xyzbca_trsrn_kins.x-rot-axis 300
setp xyzbca_trsrn_kins.z-rot-axis 400
setp xyzbca_trsrn_kins.tool-offset-z 50
setp xyzbca_trsrn_kins.pre-rot 0.3
setp xyzbca_trsrn_kins.primary-angle 20
setp xyzbca_trsrn_kins.secondary-angle 35" \
    "joints=6 r1=3 r2=4 r3=5 check=fwd"

# polar
run "rosekins" "" "joints=3 r1=2 base=10,5,0 angles=30,-25,90,120"

# arms.  Straight or folded they are singular, so the sweep keeps clear
# of 0 and 180 on the elbow.  genserkins iterates its inverse to a
# tolerance the differences would not see through, so it is checked
# against its forward only; pumakins and three21kins answer by differencing
# their own inverse and the forward is what proves the answer.
run "scarakins" "" "joints=6 r1=1 r2=3 r3=0 base=0,0,20,0,0,0 angles=30,-25,90,120,-60"
# scorbot's inverse returns the elbow-up arm, shoulder above elbow, so the
# poses have to be ones it can return: j1 above j2, and j2 within a quarter
# turn of level
run "scorbot-kins" "" "joints=5 r1=1 base=0,70,-20,0,0 angles=40,55,70,85"
run "scorbot-kins" "" "joints=5 r1=2 base=0,80,0,0,0 angles=-60,-30,0,20"
run "pumakins" "setp pumakins.D6 50" "joints=6 r1=1 r2=2 r3=4 base=15,0,0,10,0,20 angles=20,45,-35,70"
run "three21kins" "" "joints=6 r1=1 r2=2 r3=4 base=15,0,0,10,0,20 angles=20,45,-35,70"
run "genserkins" "" "joints=9 r1=1 r2=2 r3=4 base=15,0,0,10,0,20 angles=20,45,-35,70 check=fwd"
# and with a joint counted relative to the one before it
run "genserkins" "setp genserkins.unrotate-3 1" "joints=9 r1=1 r2=2 r3=4 base=15,0,0,10,0,20 angles=20,45,-35,70 check=fwd"

# parallel machines.  The struts cannot tilt the platform far, and the
# forward of the hexapod and the pentapod iterates to a tolerance, so the
# product check on those two is held to what that tolerance allows.  The
# hexapod module runs its own forward for its GUI pins in every type, with
# whatever joint values that type has, and identity joint values are not
# strut lengths it can converge from; its identity types are the shared
# ones trivkins covers, so only its own type is checked.
run "tripodkins" \
    "setp tripodkins.Bx 2
setp tripodkins.Cx 1
setp tripodkins.Cy 2" \
    "joints=3 frompose=1 base=1,1,2"
run "lineardeltakins" "" "joints=9 frompose=1 base=20,30,-200"
run "rotarydeltakins" "" "joints=9 r1=0 r2=1 frompose=1 base=0,0,-12 angles=0,2,-3"
run "genhexkins" \
    "setp genhexkins.screw-lead 0" \
    "joints=6 r1=3 r2=4 r3=5 frompose=1 base=2,3,20 angles=0,5,-7,10 tolexp=3 types=1"
run "genhexkins" \
    "setp genhexkins.screw-lead 5" \
    "joints=6 r1=3 r2=4 r3=5 frompose=1 base=2,3,20 angles=0,5,-7,10 tolexp=3 types=1"
run "pentakins" "" "joints=5 r1=3 r2=4 frompose=1 base=10,20,0 angles=0,5,-7,10 tolexp=3"
