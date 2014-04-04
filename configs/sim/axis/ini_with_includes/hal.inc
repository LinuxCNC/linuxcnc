[HAL]
# The run script first uses halcmd to execute any HALFILE
# files, and then to execute any individual HALCMD commands.
#
# list of hal config files to run through halcmd
# files are executed in the order in which they appear
HALFILE = core_sim.hal
HALFILE = sim_spindle_encoder.hal
HALFILE = axis_manualtoolchange.hal
HALFILE = simulated_home.hal
HALUI   = halui
