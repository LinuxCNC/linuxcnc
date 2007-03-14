
Some info on the different fpga configurations:

/src/hal/drivers/m5i20
this was the only config distributed with EMC prior to 2006 December.
It has a 33MHz PWM clock, so the maximum PWM rate is about 32 kHz.
It has a bug in the secondary encoder pinout(see the manual)
When the new configs (below) are sufficiently tested and working, I expect this config can be 
deleted. The HAL driver should also be updated to allow different configs.

/src/hal/drivers/m5i20/bit/hostmot5_4.bit
this is a bugfixed hostmot-4 with a 33 MHz PWM clock.

/src/hal/drivers/m5i20/bit/hostmot5_4eh.bit
A hostmot-4 with 100 MHz PWM clock which enables up to 97 kHz PWM rate.

/src/hal/drivers/m5i20/bit/hostmot5_8.bit
A hostmot-8 with 33 MHz PWM clock.

/src/hal/drivers/m5i20/bit/hostmot5_8eh.bit
A hostmot-8 with 100 MHz PWM clock.

/src/hal/drivers/m5i20/hostmot5_src
vhdl sources for the new (Dec06) configs above. It should be possible to compile the .bit 
file for each config using these sources. The required compilers are free and available on the 
internet. <more info here about how to compile vhdl>.


So far, the hal_m5i20 driver does not support the new configs, you have to load them onto the 
fpga yourself. This is done with the m5i20cfg command followed by the desired config and the 
card number. For example:
 m5i20cfg hostmot5_4eh.bit 0
will load the 4-axis 100MHz PWM clock config onto the first m5i20 card. Note that when 
loading the hal driver you should specify loadFpga=0 (otherwise the config is overwritten with the old 
hostmot-4!)

