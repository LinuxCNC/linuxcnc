
# basic group example

from machinekit import hal
g2 = hal.Group("group1")
s1 = hal.Signal("sigs32",   hal.HAL_S32)
s2 = hal.Signal("sigfloat", hal.HAL_FLOAT)
s3 = hal.Signal("sigbit",   hal.HAL_BIT)
s4 = hal.Signal("sigu32",   hal.HAL_U32)

# by object
g2.add(s1, eps_index=2)
g2.add(s2)
# by name
g2.add("sigbit")
g2.add("sigu32")

print g2.members()
