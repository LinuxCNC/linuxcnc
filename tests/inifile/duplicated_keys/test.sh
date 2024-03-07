#!/bin/bash

inivar -ini duplicated_keys.ini -var key1
inivar -ini duplicated_keys.ini -var key1 -sec section1 -num 1
inivar -ini duplicated_keys.ini -var key1 -sec section2 -num 2
inivar -ini duplicated_keys.ini -var key1 -num 3
