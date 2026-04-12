#!/bin/bash

true > result
inivar -ini missing_section.ini -sec section1 -var key1 >> result 2>&1
inivar -ini missing_value.ini -var key1 >> result 2>&1
inivar -ini missing_section.ini -var key1 >> result 2>&1
exit 0
