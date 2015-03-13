Pror to the addition of ARM support to Machinekit the only architecture supported was x86.
This meant that all the LCNC configs in the machinekit/configs directory were a flat collection of configs.
With the addition of ARM support, some structure has been added to the config tree to make it easier to find things.
The new structure is present in the ARM sub-tree of the config dir. 
Once the Arm support is mergerd back to LCNC mainline, the existing x86 config examples can be rearranged to fit the heiarchy. 

The basic dir structure is:
machinekit/configs/<architecture>/<platform>/<hardware_interface>/<machine_config>

For example: LCNC running on an ARM9, on a BeagleBone Black, using a K9 Beaglebone Black cape, configured to drive a Shapeoko router would can be found in:
machinekit/configs/ARM/BeagelBoneBlack/K9/Shapeoko

This makes the choices that appear in the LCNC config picker have reasonably obvious names. 

Once ARM support is merged into LCNC, we expect to see these additional top level config branches:
machinekit/configs/x86/
machinekit/configs/x86/PC
machinekit/configs/x86/SIM



