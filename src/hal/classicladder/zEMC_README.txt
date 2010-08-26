File changes for EMC classicladder 7.124 from original 7.124
Jan. 2007

arith_eval.c/.h:
--- backported fix from version 7.126 to fix a crash if variable number is not a number
--- changed printf to rtapi_print on line 414

arrays.c:
--- changed extensively. Removed most allocation code besides what EMC needs for realtime and user programs. 
--- added #ifndef RTAPI around INCLUDE of files.h which caused error in realtime (no directory access in realtime)
--- this also means adding #ifndef RTAPI around 'CleanAndRemoveTmpDir' call in 'classicladder_free_all'
--- copy GeneralParams into GeneralParamsMirror (in user code only) so config window displays proper info
--- add CurentProjectFileName to infosgene array so filename is remembered when GUI is closed
--- add space for %E variables

Calc.c:
--- removed define for module and RTAI
--- removed and moved period calculation to module_hal.c
--- changed inputCountrol pin of old timers to be always true so it behaves as previous old timers. check out-CalcTypeTimer()

classicladder.c:
--- changed extensively. Removed most of initialization code besides what was added for HAL.
--- A config file can be loaded from the comand line for modbus info *depreciated*
--- to use MODBUS master use --modmaster
--- to use MODBUS slave use --modslave
--- to set debug level for rtapi_print use --debug
--- if a program was loaded previouly and you load classicladder again with a ladder program
    specified it will load it instead of ignoring it.
--- change to put CurrentProjectfileName into infosgene array
--- change to display ladder program name on section display window

classicladder.h:
--- small amount added. Add definitions for HAL s32 pins and For HAL support.
--- changed define for symbols comment length from 30 to 50 for long signal names
--- add defines for new variable %QW and %IW 
--- added external variable modmaster
--- added NBR_PHYS_WORDS_INPUTS and NBR_PHYS_WORDS_OUTPUTS for support of %WQ and %IW variables
--- added CurrentProjectFileName[ 400 ] to StrInfosGene for Filename to be stored in shared mem
--- added defines etc for %E variables

classicladder gtk.c:
--- removed define for hardware.h
--- changed gtk_exit(0) to gtk_main_quit in function QuitAppliGtk() so program returns to where we called gtk_main in 
--- run/stop and reset buttons send messages to statusbar
--- classicladder will ask to confirm quit and ( if running ) reset
--- classicladder warns you that modbus will stop if you quit GUI
--- change to display ladder program name on section display window
--- added CheckErrors() for checking %E variables so a error message can be displayed
--- added ShowErrorMessage() for same

config.c :
--- added printf so we know when a modbus config file is loading

config_gtk.c:
--- removed define for hardware.h
--- added #ifndef HAL_SUPPORT around any code for direct I/O to hide it from configure window
--- modified to show number of s32 in and out pins 
--- modified modbus page to added options for read hold register, write register(s) and echo
--- if no modbus config is loaded then the modbus config page tells you this, otherwise it displays normally.
--- added a communication page for changing com settings radio buttons change settings immediately.
--- split the i/o page into two pages to improve size of config window
--- combine the i/o page back to one :)
--- add radio buttons for selecting variable that modbus maps to
--- add spin text entry selection for port name and port speed

drawing.c:
--- added colour to variable names for input (red) and output (blue) in section display drawing area
--- change background color of section display

edit.c
--- added two calls to check for hal signal names (see GetElementPropertiesForStatusBar ) one for I, Q, and B variables and another for W variables in expressions 
--- added code to default the variable name to I or Q for simple in or out elements
--- added code for proper erasure of connection-to-top with eraser object.

edit.h
--- added prototype for ConvVarNameToHalSigName()
--- added prototype for FirstVariableInArithm()
--- added prototype for SetDefaultVariableType()

file.c
--- modified not to load info into GeneralParamsMirror because only realtime can do that
--- added function to load modbus com info
--- added call to re intialize modbus after loading a program
--- addd function to load/save MODBUS com settings

files_project.c
--- change to infosgene->CurrentProjectFileName to support filenames in shared memory

emc_mods.c and emc_mods.h:
--- added Jeffs function to check for HAL signal names (called by function GetElementPropertiesForStatusBar in edit.c) 
--- added function to check the first variable in an expression, for a HAL signal (called by function GetElementPropertiesForStatusBar in edit.c) 

manager.c:
--- added small amount INCLUDE for HAL/RTAPI support.

Module_hal.c:
--- new program based on original module adds hal support/pins initialization of realtime code etc.
--- added code to refresh rungs at period rate unless period less then 1 MS then it waits till at least 1 MS has passed

protocol_modbus_master.c
--- added code for modbus functions 1, 3, 6, 16, and 8 (read coils, read holding register, write single register, write mulitple registers, echo)
--- changed ModbusEleOffset=1 to =0 (not to modbus standard but easier and fairly common) and can change this in com page now.
--- improved debug messages so slave address and function code are easily identified
--- added a write to %E0 for modbus communication error

protocol_modbus_master.h
--- added defines for same as above

spy_vars_gtk.c:
--- changed to be able to toggle vars windows (one, the other, both , both close) by clicking the button.
--- added messages to statusbar when window toggled
--- made integer window toggle from symbol names to variable names when section display window button is toggled.
--- added colours to variables to diffientiate between in,out and internal variables
--- if the number of words defined is less then number of words normally displayed the window adjusts
--- displays max 15 word variables now.
  
symbols_gtk.c:
--- changed to show HAL signals in comment slot
--- added messages to statusbar when window toggled
--- change to have only one editable line (gets rid of mass of blank lines)

SUBMAKEFILE:
--- completely different for EMC. This makefile is for the user program only. 
--- All the DEFINEs for user space:
	-DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER
	-DGNOME_PRINT_USE
	-DGTK_INTERFACE -DGTK2
--- added emc_mods.c to compile list

vars_access.c
--- add %E %IW %QW variables

vars_names.c/.h:
--- backported fix from version 7.126 to fix a crash if varible number is not a number
--- fix so if symbol name is blank will return the variable name instead (fix crash)
--- add %E %IW %QW variables

vars_names_list.c:
--- add %E %IW %QW variables




MAKE file:
--- The makefile in source folder contains instructions for realtime program. added defines for modbus
--- All the DEFINEs for realtime:
        -DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER 

classicladder_rt.o includes:
	module_hal.o,arithm_eval.o,arrays.o,calc.o,calc_sequential.o,manager.o,symbols.o,vars_access.o





