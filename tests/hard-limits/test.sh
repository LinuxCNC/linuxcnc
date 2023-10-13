#!/bin/bash

rm -f rs274ngc.var rs274ngc.var.bak

linuxcnc -r hard-limits.ini

