#!/bin/bash

function say_done {
    espeak "done" 2> /dev/null
}
set -o monitorA

./build-debug.sh
cp position.blank position.txt
linuxcnc -r circular_arcs.ini > test.log &
python machine_setup.py $1 && say_done
fg
./save_activate.sh test.log
exit $1
