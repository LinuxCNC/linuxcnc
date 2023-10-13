#/bin/bash
cd ../../../src
#Ugly way to force rebuild of kinematics, which assumes that tp_debug isn't
#used anywhere else...
touch emc/kinematics/t*.[ch]
make EXTRA_DEBUG='-DTP_PROFILE -DTP_DEBUG -DTC_DEBUG'
