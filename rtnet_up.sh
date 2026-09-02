#! /bin/sh
ifdown enp2s0f0

modprobe rt_loopback
modprobe rtudp
modprobe rtipv4
rmmod igb
rmmod rt_igb
modprobe rt_igb
rtifconfig rteth0 up 192.168.1.120 netmask 255.255.255.254
sleep 1
rtroute solicit 192.168.1.121 dev rteth0
sleep 1
rtping 192.168.1.121

