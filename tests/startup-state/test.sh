#!/bin/bash

rm -f sim.var
cp sim.var.clean sim.var
linuxcnc -r test.ini

