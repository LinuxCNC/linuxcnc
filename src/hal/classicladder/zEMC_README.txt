File changes for EMC classicladder 7.124 from original 7.124
Jan. 2007

arrays.c:
---changed extensively. Removed most allocation code besides what EMC needs for realtime and user programs. 
--- added #ifndef RTAPI around INCLUDE of files.h which caused error in realtime (no directory access in realtime)
--- this also means adding #ifndef RTAPI around 'CleanAndRemoveTmpDir' call in 'classicladder_free_all'

Calc.c:
--- added period to ClassicLadder_RefreshAllSections function:   InfosGene->GeneralParams.PeriodicRefreshMilliSecs=period/ 1000000;
--- changed inputCountrol pin of old timers to be always true so it behaves as previous old timers. check out-CalcTypeTimer()
Calc.h:
--- added period to ClassicLadder_RefreshAllSections function prototype

classicladder.c:
--- changed extensively. Removed most of initialization code besides what was added for HAL.

classicladder.h:
 ---small amount added. Add definitions for HAL s32 pins and For HAL support.

manager.c:
---added small amount INCLUDE for HAL/RTAPI support.

Module_hal.c:
---  new program based on original module adds hal support/pins initialization of realtime code etc.

classicladder gtk.c:
--- removed define for hardware.h

config_gtk.c:
--- removed define for hardware.h


SUBMAKEFILE:
--- completely different for EMC. This makefile is for the user program only. 
---All the DEFINEs for user space:
	-DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT
	-DGNOME_PRINT_USE
	-DGTK_INTERFACE -DGTK2

MAKE file:
--- The makefile in source folder contains instructions for realtime program. added defines for modbus

---All the DEFINEs for realtime:
        -DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER

classicladder_rt.o includes:
	module_hal.o,arithm_eval.o,arrays.o,calc.o,calc_sequential.o,manager.o,symbols.o,vars_access.o




