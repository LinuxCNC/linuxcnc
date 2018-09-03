# example for using Python with cython bindings as a [HAL]HALFILE
# in the ini file, add as last HALFILE:

#[HAL]
#HALFILE = haltest.py


from machinekit.halfile import rt, hal


rt.loadrt("supply")
hal.addf("supply.0.update","servo-thread")
