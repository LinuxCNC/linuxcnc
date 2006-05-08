#!/bin/bash

#this file is a poor attempt at launching halvcp,
#and binding the hal pins to the proper halui pins,
#for that to succeed you need halui running ([HAL]HALUI=halui in the ini)
#check halvcp.hal for bindings

#update the next one to fit your path, 
#it does work for me executed from this config dir
../../bin/halvcp ./halui.vcp &

#increase the sleep if you have a very slow machine
sleep 5

../../bin/halcmd -f < ./halvcp.hal &

