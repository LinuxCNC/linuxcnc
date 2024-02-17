#!/bin/bash

inivar -ini missing_section.ini -sec section1 -var key1
inivar -ini missing_value.ini -var key1
inivar -ini missing_section.ini -var key1
