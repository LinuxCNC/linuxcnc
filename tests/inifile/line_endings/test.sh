#!/bin/bash

# Manually create INI files with different line endings.
lf="[section1]\nkey1=value1\n"
cr="[section1]\rkey1=value1\r"
crlf="[section1]\r\nkey1=value1\r\n"

echo -e "$lf" > "lf.ini"
echo -e "$cr" > "cr.ini"
echo -e "$crlf" > "crlf.ini"

inivar -ini lf.ini -var key1    # Should be successful.
inivar -ini cr.ini -var key1    # Should fail.
inivar -ini crlf.ini -var key1  # Should be successful.

rm -f lf.ini cr.ini crlf.ini
