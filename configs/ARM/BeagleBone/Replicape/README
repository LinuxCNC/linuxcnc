Replicape Configuration for MachineKit
======================================

This is to make Replicape usable in Machinekit, essentially this contains:
* A sample HAL linking up the hardware and GPIO
* An PyVCP for temperature monitoring (adopted from CRAMPS config)
* Python HAL module for PWM controlling (Replicape uses a dedicated PWM controller)
* Python HAL module for Stepper configuration such as Enable, Microstepping, Decay, and DAC configuration for the stepper current settings.

This is developed based on the CRAMPS configuration that comes with the Machinekit.  This uses the same generic PRUSS firmware that comes with Machinekit/CRAMPS in case you are interested.

Prerequisite 
------------
* Machinekit (of course)
* Device Overlay Tree for Replicape
See README in Redeem for details https://bitbucket.org/intelligentagent/redeem/src/
* Python Module spi, smbus

Bugs / TODO
-----------
* You might need to copy the M1xx ncfiles to your PROGRAM_PREFIX folder. The 'getp' halcmd in them should be changed to 'gets'.
* The acceleration, velocity, microstepping, stepper current settings must be tuned for your system
