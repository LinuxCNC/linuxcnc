Installation requirements for using designer to create and/or edit QtVCP screens.

All the requirements listed below can be installed by using the install_script located in the same directory as this file.

Requirements for viewing QtVCP screens:

Minimum:
python3-opengl
python3-opencv
python3-pyqt5
python3-pyqt5.qsci
python3-pyqt5.qtsvg
python3-pyqt5.qtopengl

Optional Recommended:
gstreamer1.0-tools
espeak
espeak-ng
sound-theme-freedesktop
python3-dbus
python3-dbus.mainloop.pyqt5
python3-espeak
python3-pyqt5.qtwebkit
python3-xlib
python3-numpy
python3-cairo
python3-gi-cairo

Requirements for using designer:
qttools5-dev
qttools5-dev-tools
libpython3-dev
pyqt5-dev-tools

Create the plugin directory for designer to search:
mkdir -p ~/.designer/plugins/python

The <pluginfile> is dependent on the type of LinuxCNC installation.
For a package installation:
/usr/lib/python3/dist-packages/qtvcp/plugins/qtvcp_pluin.py

For a RIP installation:
~/linuxcnc-dev/lib/python/qtvcp/plugins/qtvcp_pluin.py

Designer can now be run and the LinuxCNC widgets will be available.

For a RIP install only:
source linuxcnc-dev/scripts/rip-environment

Start designer with:
designer -qt=5
