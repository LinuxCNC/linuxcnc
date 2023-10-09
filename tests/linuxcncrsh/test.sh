#!/bin/bash

rm -f gcode-output

if nc -z localhost 5007; then
    echo "Process already listening on port 5007. Exiting"
    exit 1
fi

linuxcnc -r linuxcncrsh-test.ini &


# let linuxcnc come up
TOGO=80
while [  $TOGO -gt 0 ]; do
    echo trying to connect to linuxcncrsh TOGO=$TOGO
    if nc -z localhost 5007; then
        break
    fi
    sleep 0.25
    TOGO=$(($TOGO - 1))
done
if [  $TOGO -eq 0 ]; then
    echo connection to linuxcncrsh timed out
    exit 1
fi


(
    # initialize
    echo hello EMC mt 1.0
    echo set enable EMCTOO
    echo get enable
    echo get debug
    echo set verbose on
    echo get verbose
    echo set echo off
    echo get echo
    # ask linuxcncrsh to not read the next command until it's done running
    # the current one
    echo set set_wait done
    echo get set_wait

    # check default global settings
    echo get plat
    echo get update
    echo get inifile

    # test commands failing for machine not running
    echo set mode mdi

    # test misc. get commands
    echo get abs_act_pos
    echo get abs_cmd_pos
    echo get angular_unit_conversion
    echo get comm_mode
    echo get comm_prot
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
    echo get pos_offset
    echo get probe_tripped
    echo get probe_value
    echo get program
    echo get program_angular_units
    echo get program_codes
    echo get program_line
    echo get program_linear_units
    echo get program_status
    echo get program_units
    echo get rel_act_pos
    echo get rel_cmd_pos
    echo get set_wait
    echo get teleop_enable
    echo get timeout
    echo get tool
    echo get tool_offset
    echo get update
    echo get user_angular_units
    echo get user_linear_units

    # prepare machine
    echo get estop
    echo set estop off
    echo get estop
    echo get machine
    echo set machine on
    echo get machine
    echo get mode
    echo set mode manual
    echo get mode

    # test spindle command
    echo get spindle                # default setting
    echo set spindle forward        # turn on all w/o param
    echo get spindle                # check all w/o param
    echo set spindle off -1         # turn off all w/param
    echo get spindle -1             # check all w/param
    echo set spindle forward 99     # turn on illegal spindle

    # test brake command
    echo get brake                  # default setting
    echo set brake on               # turn on all w/o param
    echo get brake                  # check all w/o param
    echo set brake off -1           # turn off all w/param
    echo get brake -1               # check all w/param
    echo set brake forward 99       # turn on illegal spindle

    echo set mode mdi
    echo get mode

    echo set mdi m100 p-1 q-2
    sleep 1

    # here comes a big blob
    dd bs=4096 if=lots-of-gcode

    echo set mdi m100 p-3 q-4

    echo shutdown
) | nc -v localhost 5007 > telnet-output


# wait for linuxcnc to finish
wait

exit 0
