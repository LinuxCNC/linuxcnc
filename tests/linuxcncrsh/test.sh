#!/bin/bash

set -e

rm -f gcode-output

if nc -z localhost 5007; then
    echo "Process already listening on port 5007. Exiting"
    exit 1
fi

linuxcnc -r linuxcncrsh-test.ini &


# let linuxcnc come up
TOGO=80
while [  $TOGO -gt 0 ]; do
    echo "trying to connect to linuxcncrsh TOGO=$TOGO"
    if nc -z localhost 5007; then
        break
    fi
    sleep 0.25
    TOGO=$((TOGO - 1))
done
if [  $TOGO -eq 0 ]; then
    echo connection to linuxcncrsh timed out
    exit 1
fi

# test set command by
# - getting the key's old value before setting
# - setting the new value
# - getting the new value
# - collect possible error from linuxcncsvr
function testSet() {
    # get before value
    echo "get $1"
    # set cmd
    echo "set $*"
    # get after value
    echo "get $1"
    # get error from server (or OK)
    echo "get error"
}

function testSetOnly() {
    # set cmd
    echo "set $*"
    # get error from server (or OK)
    echo "get error"
}

(
    # initialize
    echo set timestamp off
    #echo set timestamp on
    # We don't really care about versioning this API
    echo hello EMC mt 1.1
    # Providing a bad password will disable and unlink
    echo hello BADPASSWORD mt 1.1
    echo get debug
    # relink
    echo hello EMC mt
    # Don't echo enable password
    echo set echo off
    testSet enable EMCTOO
    echo set echo on
    echo get debug
    testSet verbose on
    testSet echo off
    # ask linuxcncrsh to not read the next command until it's done running
    # the current one
    testSet wait_mode "done"

    # Get some INI file entries
    echo get ini DISPLAY DISPLAY
    echo get ini SPINDLES TRAJ
    echo get ini AVAILABLE NOT
    # Can't use 'get inifile' because it returns an absolute path that varies
    # with the test environment.
    # echo get inifile

    # check default global settings
    echo get plat
    echo get update

    # test commands that fail when machine not running
    testSet mode mdi

    # prepare machine
    testSet estop off
    testSet machine on

    # test if probing in manual mode fails
    testSet mode manual
    echo get pos_offset
    echo get probe_tripped
    echo get probe_value
    testSetOnly probe_clear
    testSetOnly probe 0 0 0                # <x> <y> <z>
    testSetOnly probe_clear

    # test if probing before homing fails (not)
    testSet mode mdi
    # probe with zero feed fails
    testSetOnly probe 0 0 0                # <x> <y> <z>
    echo set mdi f10
    # probe with no movement fails
    testSetOnly probe 0 0 0                # <x> <y> <z>
    testSetOnly probe 0 0 0.01             # <x> <y> <z>
    echo get probe_tripped
    testSetOnly probe_clear

    # do homing
    testSet mode manual
    echo get joint_homed                   # Current home status
    testSetOnly joint_home 0               # Home joint
    echo set joint_wait_homed 0            # Homing takes time
    echo get joint_homed                   # New home status

    testSetOnly joint_home 1
    echo set joint_wait_homed 1
    echo get joint_homed

    testSetOnly joint_home 2
    echo set joint_wait_homed 2
    echo get joint_homed

    # These should fail, there are only three joints
    # (also tests alias of joint_home)
    testSetOnly home 3
    testSetOnly home 4
    testSetOnly home 5

    echo set wait_heartbeat 10000         # must fail: would be 10 seconds

    echo set echo on
    echo set wait_heartbeat
    testSet teleop_enable off              # Or we can't unhome
    echo get joint_homed
    testSetOnly joint_unhome 1
    echo get joint_homed
    testSetOnly joint_home 1
    echo set joint_wait_homed 1
    echo get joint_homed
    # There is a race on get teleop_enable. After we homed, it may take a while
    # for teleop mode to become active again.
    # The SET JOINT_WAIT_HOMED already waited a heartbeat for us
    # echo set wait_heartbeat
    echo get teleop_enable
    echo set echo off

    # test probing
    testSet mode mdi
    testSetOnly probe 0 0 0.02                # <x> <y> <z>
    echo get probe_tripped
    testSetOnly probe_clear

    testSet mode manual
    testSet teleop_enable on
    testSet mode mdi
    testSetOnly probe 0 0 0                # <x> <y> <z>
    echo get probe_tripped
    testSetOnly probe_clear

    # test spindle command
    testSet mode manual
    testSet spindle forward                # turn on all w/o param
    testSet spindle off -1                 # turn off all w/param
    echo get spindle -1                    # check all w/param
    testSet spindle forward 99             # turn on illegal spindle

    # test brake command
    testSet brake on                       # turn on all w/o param
    testSet brake off -1                   # turn off all w/param
    echo get brake -1                      # check all w/param
    testSet brake forward 99               # turn on illegal spindle

    # test pause
    testSet wait_mode received             # otherwise pause will stall
    testSetOnly pause
    testSet wait_mode "done"
    testSetOnly resume

    # test g-code
    echo set mode mdi
    echo set mdi f10

    echo get abs_act_pos
    echo set mdi g1 x0.3 y0.2 z-0.1        # Feed move
    echo set wait_heartbeat 5              # End of move may take some time
    echo get abs_act_pos

    echo set mdi g0x0y0z0                  # Rapid back to home
    echo set mdi m100 p-1 q-2

    # here comes a big blob
    dd bs=4096 if=lots-of-gcode
    echo set mdi m100 p-3 q-4

    # test misc. get commands
    echo get error
    echo get abs_act_pos
    echo get abs_cmd_pos
    echo get angular_unit_conversion
    echo get display_angular_units
    echo get display_linear_units
    echo get error
    echo get feed_override
    echo get flood
    echo get joint_fault
    echo get joint_homed
    echo get joint_limit
    echo get joint_pos
    echo get joint_type
    echo get joint_units
    echo get kinematics_type
    echo get linear_unit_conversion
    echo get mist
    echo get operator_display
    echo get operator_text
    echo get optional_stop
    echo get override_limits
    echo get program
    echo get program_angular_units
    echo get program_codes
    echo get program_line
    echo get program_linear_units
    echo get program_status
    echo get program_units
    echo get rel_act_pos
    echo get rel_cmd_pos
    echo get wait_mode
    echo get teleop_enable
    echo get timeout
    echo get tool
    echo get tool_offset
    echo get update
    echo get user_angular_units
    echo get user_linear_units

    # test misc. set commands
    testSetOnly abort
    testSet angular_unit_conversion deg    # <Deg | Rad | Grad | Auto | Custom>
    testSet feed_override 100              # <Percent>
    testSet flood on                       # <On | Off>
    testSetOnly jog 0 1                    # <Axis No, Speed>
    testSetOnly jog_incr 0 1 1             # <Axis No, Speed, Distance>
    testSetOnly jog_stop 0                 # <Joint No|Axis letter>
    testSet linear_unit_conversion mm      # <Inch | CM | MM | Auto | Custom>
    testSet mist on                        # <On | Off>
    testSet optional_stop 1                # <none | 0 | 1>

    # These are a bit awkward to test
    #testSet load_tool_table <Table name>
    #testSet Open <File path / name>
    #testSet run <Line No>

    # Testing set override_limits results in command line message:
    #   can't do that (EMC_JOINT_OVERRIDE_LIMITS:129) in MDI mode
    testSet override_limits off            # <On | Off>

    # Testing step results in a command line message:
    #   can't do that (EMC_TASK_PLAN_STEP:511) in MDI mode
    testSetOnly step

    testSetOnly task_plan_init
    testSet teleop_enable off
    testSet timeout 10
    #testSet tool_offset 1 0.5 0.25
    testSet update auto                    # <None | Auto>
    testSet wait "done"                    # Wait for last command to be processed

    echo set estop on
    echo get estop

    # Shutdown will terminate linuxcncrsh and thus terminates everything
    # because it is set as our DISPLAY program.
    echo shutdown
) | nc -v localhost 5007 > telnet-output

# wait for linuxcnc to finish
wait

exit 0
