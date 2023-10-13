cp position.blank position.txt
mv constraints.log constraints_old.log
linuxcnc circular_arcs.ini > test.log
./process_runlog.sh test.log movement.txt
if [ -a movement.txt ] 
then
    octave --persist ./octave/plot_movement.m
fi
