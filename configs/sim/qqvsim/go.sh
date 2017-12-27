# . ~/proj/machinekit.upstream/scripts/rip-environment
. ~/proj/machinekit/scripts/rip-environment

linuxcnc_stop
sudo cp /dev/null /var/log/linuxcnc.log

# export FLAVOR=rt-preempt
export FLAVOR=posix
export NOSIGHDLR=1
ulimit -S -c unlimited # no limit on core dump size

# DEBUG=5 linuxcnc axis_jerk_mm.ini | tee run.log

# for remote-ui
mklauncher . &
python2 run.py   # set_debug_level() inside run.py
