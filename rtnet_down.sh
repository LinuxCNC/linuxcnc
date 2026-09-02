#! /bin/sh
rtifconfig rteth0 down

rmmod rt_igb
modprobe igb
ifup enp2s0f0
