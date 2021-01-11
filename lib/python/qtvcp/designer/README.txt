These folders hold the required library (in binary form) for designer to use 
python2 widgets.

need for displayimg pyqt widgets:
python-pyqt5
python-pyqt5.qsci
python.pyqt5.qtopengl
python-pyqt5.qtsvg
python-opengl
python-opencv

optional:
espeak
espeak-ng-espeak
python-dbus.mainloop.pyqt

you must have designer installed:
sudo apt-get install qttools5-dev-tools
sudo apt-get install qttools5.dev
sudo apt-get install libpython-dev

you must copy that proper version of libpyqt5_py2.so to the folder:
/usr/lib/x86_64-linux-gnu/qt5/plugins/designer
(x86_64-linux-gnu might be called something slightly different 
on different systems)

The libpyqt5_py2.so must be the first python library to be found in the folder.
Some systems have the python3 library - libpyqt5.so - file in the folder.
You must rename one of the files so it is found first.
Renaming the python3 version to libpyqt5_py3.so should do this.

You will require super user privileges to copy/rename the file to the folder.

This file might be included in linuxcnc at this location:

in a RIP version:
lib/python/qtvcp/designer/x86_64
or in an installed version:
/usr/lib/python2.7/dist-packages/qtvcp/designer/x86_64/

You must pick the series 5.5 or 5.7 or 5.9 of Qt
currently Debian stretch uses 5.7, Mint 12 uses 5.5
if in doubt check the version qt5 on the system
If you need to build the library for a certain version see:
https://gist.github.com/KurtJacobson/34a2e45ea2227ba58702fc1cb0372c40

then you must link the qtvcp_plugin.py to the folder that designer will search.
(The link name must have .py as an ending.)
Qtvcp_plugin can be found at:
in RIP version:
lib/python/qtvcp/plugins
in an installed version:
/usr/lib/python2.7/dist-packages/qtvcp/plugins

The link can be placed in:
/usr/lib/x86_64-linux-gnu/qt5/plugins/designer/python
or 
~/.designer/plugins/python

open a terminal, set the environment for linuxcnc (. scripts/rip-environment if RIP)
then load designer with : designer -qt=5


