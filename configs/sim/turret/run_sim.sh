#!/bin/bash
# Run the turret simulation (mechanism + controller) under halrun.
#
#   ./run_sim.sh        # F1: Python manager (turret.manager)
#   ./run_sim.sh f2     # F2: compiled component (loadusr turret)
set -e
cd "$(dirname "$0")"

# RIP environment (interpreter and lib/python on PYTHONPATH)
if [ -f ../../../scripts/rip-environment ]; then
    . ../../../scripts/rip-environment >/dev/null
fi

if [ "$1" = "f2" ]; then
    exec halrun -f turret_sim_f2.hal
fi
exec halrun -f turret_sim.hal
