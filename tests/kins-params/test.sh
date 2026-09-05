#!/bin/bash
set -e

${SUDO} halcompile --install paritycheck.c >/dev/null

# One hal file per module.  paritycheck evaluates the module in realtime
# through the classic entry points and publishes the answers; check.py
# evaluates it through the non-realtime loader, kinsDescribe() and the
# parameter block, and compares.  Where they disagree the module keeps
# state its table does not declare.
# ONLY=<module> in the environment runs the entries for that module alone
run() {
    local loadrt="$1" setp="$2" parms="$3" ktype="$4"
    local module coords sparm joints frompose pose jnt hal tok
    case "$loadrt" in "${ONLY:-}"*) ;; *) return 0 ;; esac
    module=${loadrt%% *}
    coords=""; sparm=""
    for tok in $loadrt; do
        case "$tok" in
            coordinates=*) coords=${tok#coordinates=} ;;
            sparm=*) sparm=${tok#sparm=} ;;
        esac
    done
    joints=3; frompose=0; pose="0,0,0,0,0,0,0,0,0"; jnt="10,20,30,40,50,60,70,80,90"
    for tok in $parms; do
        case "$tok" in
            joints=*) joints=${tok#joints=} ;;
            frompose=*) frompose=${tok#frompose=} ;;
            pose=*) pose=${tok#pose=} ;;
            jnt=*) jnt=${tok#jnt=} ;;
        esac
    done
    hal=$(mktemp --suffix=.hal)
    { printf 'loadrt %s\n' "$loadrt"
      printf '%s\n' "$setp"
      printf 'loadrt paritycheck %s ktype=%s\n' "$parms" "${ktype:-0}"
      # halcmd keeps quotes, so an absent value travels as a dash
      printf 'loadusr -w python3 check.py %s %s %s %s %s %s %s %s\n' \
             "$module" "$joints" "${coords:--}" "${ktype:-0}" "$frompose" "$pose" "$jnt" "${sparm:--}"
    } > "$hal"
    echo "=== $loadrt type ${ktype:-0}"
    halrun -f "$hal"
    rm -f "$hal"
}

# identity, a gantry included
run "trivkins coordinates=XYZ" "" "joints=3 jnt=10,20,30"
run "trivkins coordinates=XYZY kinstype=BOTH" "" "joints=4 jnt=10,20,30,20"
run "trivkins coordinates=XYZABCUVW" "" "joints=9"
run "userkins" "" "joints=3 jnt=10,20,30"
run "millturn" "" "joints=4 jnt=10,20,30,40"
run "millturn" "" "joints=4 jnt=10,20,30,40" 1

# linear maps and one rotation
run "corexykins" "" "joints=9"
run "rotatekins" "" "joints=9"
run "matrixkins" \
    "setp matrixkins.C_xy 0.02
setp matrixkins.C_xz -0.01
setp matrixkins.C_yx 0.03
setp matrixkins.C_yz 0.015
setp matrixkins.C_zx -0.02
setp matrixkins.C_zy 0.01
setp matrixkins.C_zz 1.001" \
    "joints=9"

# tables and heads, offsets set so no term drops out
run "maxkins" \
    "setp maxkins.pivot-length 100" \
    "joints=9 jnt=10,20,30,0,15,25,7,0,3"

run "5axiskins coordinates=XYZBCW" "" "joints=6 jnt=10,20,30,15,25,5"
run "5axiskins coordinates=XYZBCW sparm=identityfirst" "" "joints=6 jnt=10,20,30,15,25,5" 1

run "xyzac-trt-kins coordinates=XYZAC" \
    "setp xyzac-trt-kins.y-offset 3
setp xyzac-trt-kins.z-offset 11
setp xyzac-trt-kins.tool-offset 7
setp xyzac-trt-kins.x-rot-point 1
setp xyzac-trt-kins.y-rot-point 2
setp xyzac-trt-kins.z-rot-point 5" \
    "joints=5 jnt=10,20,30,15,25"

run "xyzbc-trt-kins coordinates=XYZBC" \
    "setp xyzbc-trt-kins.conventional-directions 1
setp xyzbc-trt-kins.x-offset 3
setp xyzbc-trt-kins.z-offset 11
setp xyzbc-trt-kins.tool-offset 7
setp xyzbc-trt-kins.x-rot-point 1
setp xyzbc-trt-kins.y-rot-point 2
setp xyzbc-trt-kins.z-rot-point 5" \
    "joints=5 jnt=10,20,30,15,25"

run "xyzab_tdr_kins" \
    "setp xyzab_tdr_kins.x-offset 3
setp xyzab_tdr_kins.z-offset 11
setp xyzab_tdr_kins.tool-offset-z 7
setp xyzab_tdr_kins.x-rot-point 1
setp xyzab_tdr_kins.y-rot-point 2
setp xyzab_tdr_kins.z-rot-point 5" \
    "joints=5 jnt=10,20,30,15,25" 1

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
    "joints=6 jnt=10,20,30,15,25,35" 1

run "xyzacb_trsrn" \
    "setp xyzacb_trsrn_kins.nut-angle 45
setp xyzacb_trsrn_kins.y-pivot 100
setp xyzacb_trsrn_kins.z-pivot 200
setp xyzacb_trsrn_kins.pre-rot 0.3
setp xyzacb_trsrn_kins.primary-angle 20
setp xyzacb_trsrn_kins.secondary-angle 35" \
    "joints=6 jnt=10,20,30,15,25,35" 2

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
    "joints=6 jnt=10,20,30,15,25,35" 1

# polar
run "rosekins" "" "joints=3 jnt=10,5,30"

# arms
run "scarakins" "" "joints=6 jnt=30,40,20,10,0,0"
run "scorbot-kins" "" "joints=5 jnt=40,60,-20,0,0"
run "pumakins" "setp pumakins.D6 50" "joints=6 jnt=15,20,-35,10,70,20"
run "three21kins" "" "joints=6 jnt=15,20,-35,10,70,20"
run "genserkins" "" "joints=9 jnt=15,20,-35,10,70,20,0,0,0"
run "genserkins" "setp genserkins.unrotate-3 1" "joints=9 jnt=15,20,-35,10,70,20,0,0,0"

# parallel machines, from a pose the forward can be seeded with
run "tripodkins" \
    "setp tripodkins.Bx 2
setp tripodkins.Cx 1
setp tripodkins.Cy 2" \
    "joints=3 frompose=1 pose=1,1,2"
run "lineardeltakins" "" "joints=9 frompose=1 pose=20,30,-200"
run "rotarydeltakins" "" "joints=9 frompose=1 pose=0,0,-12"
run "genhexkins" "setp genhexkins.screw-lead 0" "joints=6 frompose=1 pose=2,3,20,0,5,-7"
run "genhexkins" "setp genhexkins.screw-lead 5" "joints=6 frompose=1 pose=2,3,20,0,5,-7"
run "pentakins" "" "joints=5 frompose=1 pose=10,20,0,5,-7"
