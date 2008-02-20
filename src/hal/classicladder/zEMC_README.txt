File changes for EMC classicladder 7.124 from original 7.124
Jan. 2007

arrays.c:
--- changed extensively. Removed most allocation code besides what EMC needs for realtime and user programs. 
--- added #ifndef RTAPI around INCLUDE of files.h which caused error in realtime (no directory access in realtime)
--- this also means adding #ifndef RTAPI around 'CleanAndRemoveTmpDir' call in 'classicladder_free_all'
--- copy GeneralParams into GeneralParamsMirror (in user code only) so config window displays properly

Calc.c:
---removed and moved period calculation to module_hal.c
--- changed inputCountrol pin of old timers to be always true so it behaves as previous old timers. check out-CalcTypeTimer()

classicladder.c:
--- changed extensively. Removed most of initialization code besides what was added for HAL.
--- A config file can be loaded from the comand line for modbus info

classicladder.h:
 ---small amount added. Add definitions for HAL s32 pins and For HAL support.

classicladder gtk.c:
--- removed define for hardware.h
--- changed gtk_exit(0) to gtk_main_quit in function QuitAppliGtk() so program returns to where we called gtk_main in classicladder.c

config.c :
--- added printf so we know when a modbus config file is loading

config_gtk.c:
--- removed define for hardware.h
--- added #ifndef HAL_SUPPORT around any code for direct I/O to hide it from configue window
--- modified to show number of s32 in and out pins 

edit.c
--- added two calls to check for hal signal names (see GetElementPropertiesForStatusBar ) one for I, Q, and B variables and another for W variables in expressions 

edit.h
--- added prototype for ConvVarNameToHalSigName()
--- added prototype for FirstVariableInArithm();

file.c
--- modified not to load info into GeneralParamsMirror because only realtime can do that
--- 
emc_mods.c and emc_mods.h:
----added Jeffs function to check for HAL signal names (called by function GetElementPropertiesForStatusBar in edit.c) 
--- added function to check the first variable in an expression, for a HAL signal (called by function GetElementPropertiesForStatusBar in edit.c) 

manager.c:
---added small amount INCLUDE for HAL/RTAPI support.

Module_hal.c:
---  new program based on original module adds hal support/pins initialization of realtime code etc.
--- added code to refresh rungs at period rate unless period less then 1 MS then it waits till at least 1 MS has passed

spy_vars_gtk.c:
--- changed to be able to toggle vars windows (one, the other, both , both close) by clicking the button.

symbols_gtk.c:
--- changed to show HAL signals in comment slot

SUBMAKEFILE:
--- completely different for EMC. This makefile is for the user program only. 
--- All the DEFINEs for user space:
	-DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER
	-DGNOME_PRINT_USE
	-DGTK_INTERFACE -DGTK2
--- added emc_mods.c to compile list

MAKE file:
--- The makefile in source folder contains instructions for realtime program. added defines for modbus
--- All the DEFINEs for realtime:
        -DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER 

classicladder_rt.o includes:
	module_hal.o,arithm_eval.o,arrays.o,calc.o,calc_sequential.o,manager.o,symbols.o,vars_access.o


changed classicladder to be able to load modbus configs


