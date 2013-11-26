cp position.blank position.txt
linuxcnc circular_arcs.ini > lcnc-runlog.txt
awk '/total/ {print $2,$6}' lcnc-runlog.txt > movement.txt 
octave --persist ./octave/plot_movement.m
