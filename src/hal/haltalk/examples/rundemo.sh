#!/bin/bash
export DEBUG=5

. /home/machinekit/machinekit/scripts/rip-environment

# make msgd builtin webserver to respond on 192.168.7.1
#export MSGD_OPTS="--wsdebug 255 --wwwdir $EMC2_HOME/src/rtapi/www"
export MSGD_OPTS="--wsdebug 15 --wwwdir $EMC2_HOME/src/rtapi/www"

# if halscope should connect via X somewhere else, say so here:
#export DISPLAY=1.2.3.4:0.0
#mah: export DISPLAY=193.228.47.195:0.0

# kill any current sessions
realtime stop

cd  $EMC2_HOME/src/hal/haltalk/examples

# Start the config server
#cd $EMC2_HOME/src/machinetalk/config-service/
#python configserver.py apps.ini &
#CONFIG_SERVER_PID=$!
#cd -

# this will leave a halcmd prompt - to check status etc
# to exit the demo just hit ^D

halrun -I motorctrl.hal

# cleanup
#kill $CONFIG_SERVER_PID

exit 0
