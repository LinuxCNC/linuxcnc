cp position.blank position.txt
linuxcnc circular_arcs.ini > lcnc-runlog.txt
process_runlog.sh lcnc-runlog.txt movement.txt
if [ -a movement.txt ] then
    octave --persist ./octave/plot_movement.m
fi
