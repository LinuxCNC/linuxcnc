from hal import *; import time
h = component("test");
h.newpin("i", HAL_S32, HAL_RD);
h.newpin("f", HAL_FLOAT, HAL_RD);
h.newpin("u8", HAL_U8, HAL_RD);
while 1:
    h.i += 1;
    h.f = h.i * .3125
    h.u8 = (7*h.i) % 256
    time.sleep(.1)
