#!/bin/bash

rm -f sim.var
cp sim.var.orig sim.var
linuxcnc -r test.ini

