from hal import *; import time
h = component("test");
h.newpin("i", HAL_S32, HAL_OUT);
h.newpin("f", HAL_FLOAT, HAL_OUT);
h.newpin("u32", HAL_U32, HAL_OUT);
while 1:
    h.i += 1;
    h.f = h.i * .3125
    h.u32 = (7*h.i) % 256
    time.sleep(.1)
