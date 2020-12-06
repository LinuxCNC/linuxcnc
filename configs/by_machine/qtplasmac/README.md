# qtplasmac
A Qtvcp GUI for PlasmaC based on a theme by pinder on the LinuxCNC forum

Last update: 3 Dec 2020

This GUI is in development and is not yet fully functional but can be opened as a LinuxCNC sim to get an idea of how it looks and feels. 

Most functions are operational with the exception of conversational, in which only circles are finished.

If you have LinuxCNC installed you could do either of:
1. copy this structure to a directory somewhere and run linuxcnc pointing to qtplasmac.ini.
2. clone this with git clone https://github.com/phillc54/qtplasmac.git then run linuxcnc pointing to qtplasmac.ini.

Cloning gives the added benefit of easy updates as the GUI is developed further although you will lose any saved preferences and they will resort to the defaults.

If you do want to keep any saved preferences then make a copy of qtplasmac.prefs and copy it back to the updated directory.

There are three display formats:
1. 16:9 with a minimum resolution of 1366 x 768
2. 9:16 with a minimum resolution of 768 x 1366
3. 4:3 with a minimum resolution of 1024 x 768

These formats can be changed by editing the [DISPLAY] section of qtplasmac.ini.

*16:9 Format*

<img src="https://github.com/phillc54/qtplasmac/blob/main/qtplasmac/images/qtplasmac_16x9.png" width="600" height="338"/>

*9:16 Format*

<img src="https://github.com/phillc54/qtplasmac/blob/main/qtplasmac/images/qtplasmac_9x16.png" width="338" height="600"/>

*4:3 Format*

<img src="https://github.com/phillc54/qtplasmac/blob/main/qtplasmac/images/qtplasmac_4x3.png" width="450" height="338"/>
