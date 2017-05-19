#!/bin/bash

rm -f sim.var
cp sim.var.pre sim.var
linuxcnc -r test.ini

