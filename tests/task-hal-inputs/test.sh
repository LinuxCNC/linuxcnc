#!/bin/bash

cp -f tool.tbl.original tool.tbl

exec linuxcnc -r task-hal-inputs.ini
