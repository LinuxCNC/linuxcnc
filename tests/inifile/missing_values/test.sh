#!/bin/bash

# We don't want a single redirect because the output must be very specific.
# shellcheck disable=SC2129
true > result
inivar -ini missing_section.ini -sec section1 -var key1 >> result 2>&1
inivar -ini missing_value.ini -var key1 >> result 2>&1
inivar -ini missing_section.ini -var key1 >> result 2>&1
exit 0
