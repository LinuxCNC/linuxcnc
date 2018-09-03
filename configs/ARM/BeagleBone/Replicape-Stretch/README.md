Replicape Configuration for Debian Stretch and 4.14.x kernel with Delta printer support
=======================================================================================

This is an update to the work of *Sam Wong* on a fork from github.com/sam0737/machinekit-replicape, which makes the Replicape board useful for Machinekit and 3D printers in velocity extrusion mode.  
This configuration is adapted) to Machinekit on Debian Stretch 
with an rt-preempt kernel and extended in functionality as described below, notably with support for Delta printers.

This is to make the Replicape board usable in Machinekit, essentially this contains:
 
* Python HAL script linking up the hardware, GPIO, velocity extrusion controlling and exposing remote UI HAL.
* Python HAL module for PWM controlling (Replicape uses a dedicated PWM controller)
* Python HAL module for Stepper configuration such as Enable, Microstepping, Decay, and DAC configuration for the stepper current settings.
* Reprap GCode remap

Main changes compared to the master branch of github.com/sam0737/machinekit-replicape
-------------------------------------------------------------------------------------

* adapt to Machinekit on Debian Stretch with 4.14.x rt-preempt kernel and u-boot overlays:
  
    * using rt-preempt instead of xenomai
    * add BOARD_REV variable in ini-file, as u-boot overlays seem not to publish
   the Replicape board version in dmesg or cmdline
    * check for installed REPLICAPE overlay in /proc/cmdline
    * export GPIO pins used by pru stepgen (in "set_pru_gpio.sh", not done by REPLICAPE overlay)
    * change "localpincount" to "local_pincount" in icomps to make them compile
    
* use the stepgen.xx.minvel pin to avoid "pru hunting"
   
* add support for delta printers (lineardeltakins)
    * detected by looking for DELTA_R in ini file
    * add cartesian HOME-DELTA_Z in ini file so that G28 can be modified to go to Z coordinate at top of build space
    instead of Z=0
* copy more G- and M-code mappings to ../subroutines from /usr/share/linuxcnc/ncfiles/remap-subroutines/fdm/
* implement M106 (pwm fan control)
* implement M710 (jog filament), M207 (set retract parameters), M221 (set extrusion scale)
* modify G28 to home to X0,Y0,Z(HOME-DELTA_Z) and to disable extrusion

Prerequisites 
-------------

### BeagleBoneBlack software preparation

* Machinekit on BeagleBoneBlack
    * This configuration is adapted to Machinekit on Debian Stretch with the rt-preempt kernel 4.14.18-ti-rt-r33, to be found at
  <https://elinux.org/Beagleboard:BeagleBoneBlack_Debian>
  
    * the present configuration uses an image including the machinekit package:  
        microSD/Standalone: (machinekit) Based on Debian Stretch with 4.14.x rt kernel with U-Boot Overlays, 
        specifically:  bone-debian-9.3-machinekit-armhf-2018-02-11-4gb.img.xz   
        An update to kernel 4.14.25-ti-rt-r38 was tested as well (but needed the addition of the PRU overlay, which was optional in the earlier kernel, see below). 
        Note that the Debian/machinekit packages are frequently updated.
        
    * make sure your boot loader is not too old, must support u-boot overlays, see above link for instructions to update.
    * ssh to BeagleBoneBlack from your host computer and login. If you want X11 support, you will have to "sudo apt-get install xauth" and "touch ~/.Xauthority"
    * make sure /boot/uEnv.txt has the following entries (cape_universal not enabled):
    
```
        enable_uboot_overlays=1  
        #enable_uboot_cape_universal=1
        disable_uboot_overlay_audio=1
        uboot_overlay_pru=/lib/firmware/AM335X-PRU-UIO-00A0.dtbo 
        cmdline=coherent_pool=1M net.ifnames=0 quiet  
```

   * optional: flash eMMC, instructions see above link
        
   * this config assumes that there is no "sudo" password required, which was the default in the above Debian Stretch image.

### If the cape is plugged in and powered on, it should be detected:
```
cat /proc/cmdline
```
should show an entry
```
uboot_detected_capes=BB-BONE-REPLICAP
```
You can also check the boot protocol on the BBB serial interface for the following entries (board version B3A):
```
BeagleBone: cape eeprom: i2c_probe: 0x54: /lib/firmware/BB-BONE-REPLICAP-0B3A.dtbo [0xfdcc4bf]
```
and 
```
uboot_overlays: loading /lib/firmware/BB-BONE-REPLICAP-0B3A.dtbo ...
```
* Install the following python module, which is used in the HAL
```
sudo pip install spi 
```
* Copy the Replicape-Stretch configuration to your machinekit configs directory and
```
cd ~/machinekit/configs/Replicape-Stretch
```
* Edit the replicape.ini file according to your needs. The included ini file is for a Kossel Mini delta printer. Be sure to set the new BOARD_REV entry to the actual Replicape board revison (B3A or A4A) of your hardware. See <http://www.machinekit.io/docs/setting-up/lineardelta-FDM-printer/> for specifics about delta printer setup.  
* make sure that ARM.Replicape.B3/replicape/set_pru_gpio.sh is executable. If not 
```
chmod +x ARM.Replicape.B3/replicape/set_pru_gpio.sh
```

* Clone Machineface under your BBB user home directory:
```
cd ~/
git clone https://github.com/machinekit/Machineface
```
* Enable Machinekit to accept remote connections: 
edit /etc/linuxcnc/machinekit.ini, change REMOTE to 1

* Install a machinekit client for your host computer from <https://github.com/machinekit/QtQuickVcp>

* start mklauncher on BBB (watch the period):
```
cd ~/machinekit/configs/machinekit-replicape
mklauncher .
```
* Start the machinekit client on your host computer. Be aware that for a delta printer the jog controls of Machineface work on the joints (carriages on the towers) and not on the cartesian x,y,z coordinates. Switching to teleop mode does not (yet) work in the version (March 2018) that I used. You can however jog the extruder. You can change the filament diameter on the Settings tab, or by adding M200 D[filament_diameter] to the start G-code.


### Slicing for velocity extrusion

* For Cura 3.1.2, machinekit velocity extrusion G-code is created with the "NGCWriter" plugin written by Alexander Rössler. As of March 19, 2018, this is now an official Cura plugin, so you can install it via Plugins > Browse Plugins. For an older version you can get NGCWriter from <https://github.com/machinekoder/NGCWriter>. Read <https://machinekoder.com/machinekit-and-cura/> for explanations. For the Linux version of Cura 3.2.1 (AppImage), copy the NGCWriter folder to ~/.local/share/cura/3.2/plugins/  
 Slice and save the file using the "RS-274 GCode file (*.ngc)" option. The NGCWriter postprocessing scripts can also be used to process GCode of other slicers.


License
-------
The MIT License (MIT)

Copyright (c) 2015 Sam Wong
 
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
