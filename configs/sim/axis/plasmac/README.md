#### EXAMPLE CONFIGURATIONS
\- metric_plasmac.ini  
\- imperial_plasmac.ini  

These example configuration files show examples of how to connect with the plasmac component for use in plasma cutting machines.  
The manual page for the plasmac component is accessed by the command 'man.plasmac'

Filename | Function
------------ | -------------
plasmac_panel.glade|a sample gladevcp panel connecting to the plasmac component.
plasmac_panel.hal|hal connections for the panel.
plasmac_panel.py|python code for the panel.
plasmac_panel.cfg|configuration settings.
plasmac_panel.mat|material file with cut parameters.

The .ini files are notated for extra the requirements for these configurations.  
The minimum .ini file requirements for the plasmac component are:  
\- [PLASMAC]MODE (only if mode 1 or mode 2)  
\- [AXIS_L]MAX_VELOCITY  
\- [AXIS_L]MAX_ACCELERATION  
\- [AXIS_L]OFFSET_AV_RATIO  
All other .ini file settings are for the example configurations.  
The .cfg file is maintained by the gladevcp.persistence module but values may be changed with any text editor.  
The .mat file is a comma delimited text file so may be edited with either a text editor or spreadsheet application.  
Lines beginning with # are ignored by the python file as is any extraneous whitespace.  
The python directory is for remapping of the F word.  
These configurations use the axis_tweaks file in lieu of ~/.axisrc, if you prefer to use a common ~/.axisrc file then copy the contents of axis_tweaks to your ~/.axisrc file and also comment out the line USER_COMMAND_FILE in the DISPLAY section of the ini file.  
The reverse run button will be enabled when reverse-run is merged into master.  

***
#### TEST PANEL

In the test directory there is a simple test panel and associated python file which can be used to test the example configuration as referenced in the ini file.  

***
#### NGC EXAMPLES

Example ngc files are in nc_files/plasmac.  

***
