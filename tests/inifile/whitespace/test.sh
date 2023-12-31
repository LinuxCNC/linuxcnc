#!/bin/bash

# Manually create INI file with trailing whitespace.
ini_content="[section1]   \nkey1=value1\t\nkey2=value2    \n"
echo -e "$ini_content" > "trailing_whitespace.ini"

inivar -ini leading_whitespace.ini -var key1
inivar -ini leading_whitespace.ini -sec section1 -var key1
inivar -ini space_between_equal.ini -var key1
inivar -ini trailing_whitespace.ini -var key1
inivar -ini trailing_whitespace.ini -var key2

rm -f trailing_whitespace.ini
