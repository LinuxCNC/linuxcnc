#!/bin/bash
if [ "$1" = "float" -o "$2" = "float" ]; then F=""; else F="nofp"; fi
sed -e "s,@IN@,$1,g; s,@OUT@,$2,g; s,@CC@,$3,g; s,@MIN@,${4-0},g; s,@MAX@,${5-0},g; s,@FP@,$F,g;"
