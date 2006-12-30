
Some info on the different fpga configurations:

/src/hal/drirvers/m5i20
this was the only config distributed with EMC prior to 2006 December.
It has a 33MHz PWM clock, so the maximum PWM rate is about 32 kHz.
It has a bug in the secondary encoder pinout(see the manual)
When the new configs (below) are sufficiently tested and working, I expect this config can be 
deleted.

/src/hal/drirvers/m5i20/hostmot5_4
this is a bugfixed hostmot-4 with a 33 MHz PWM clock.

/src/hal/drirvers/m5i20/hostmot5_4eh
A hostmot-4 with 100 MHz PWM clock which enables up to 97 kHz PWM rate.

/src/hal/drirvers/m5i20/hostmot5_8
A hostmot-8 with 33 MHz PWM clock.

/src/hal/drirvers/m5i20/hostmot5_8eh
A hostmot-8 with 100 MHz PWM clock.


