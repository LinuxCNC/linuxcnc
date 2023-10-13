Installation requirements for using QtVCP screens and panels.

All the requirements listed below can be installed by using the install_script located in the same directory as this file.

Recommended packages for viewing QtVCP screens or panels:
gstreamer1.0-tools
espeak
espeak-ng
sound-theme-freedesktop
python3-opengl
python3-pyqt5
python3-pyqt5.qsci
python3-pyqt5.qtsvg
python3-pyqt5.qtopengl
python3-opencv
python3-dbus
python3-dbus.mainloop.pyqt5
python3-espeak
python3-pyqt5.qtwebengine
python3-xlib
python3-numpy
python3-cairo
python3-gi-cairo
python3-poppler-qt5
pyqt5-dev-tools

-------------------------------------------------------------------------------

Below are requirements for using Designer to edit or create new QtVCP screens or panels:

Required packages:
qttools5-dev
qttools5-dev-tools
libpython3-dev

To enable LinuxCNC specific widgets to appear in Designer:

1. Create the plugin directory for designer to search:
    mkdir -p ~/.designer/plugins/python

2. Copy the <pluginfile> to the above directory:
    cp <pluginfile> ~/.designer/plugins/python/

The <pluginfile> is dependent on the type of LinuxCNC installation.

For a package installation:
    /usr/lib/python3/dist-packages/qtvcp/plugins/qtvcp_pluin.py

For a RIP installation:
    ~/linuxcnc-dev/lib/python/qtvcp/plugins/qtvcp_pluin.py

Designer can now be run and the LinuxCNC widgets will be available.

For a RIP install only:
    source ~/linuxcnc-dev/scripts/rip-environment

Start designer with:
    designer -qt=5
