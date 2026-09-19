#!/bin/bash
# Unit tests for the turret package (pure Python, no HAL/display needed).
set -e
cd "$(dirname "$0")"
python3 ./test_fsm.py
python3 ./test_config.py
python3 ./test_maintenance.py
python3 ./test_manager.py
python3 ./test_sim.py
python3 ./test_genconf.py
echo ok
