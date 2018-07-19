These folders hold the required library (in binary form) for designer to use 
python2 widgets.

you must have designer installed:
sudo apt-get install qttools5-dev-tools
sudo apt-get install qttools5.dev

You must pick 32 or 64 bit cpu then pick the series 5.5 or 5.7 of Qt
currently Debian stretch uses 5.7, Mint 12 uses 5.5
if in doubt check the version qt5 on the system
If you need to build the library for a certain version see:
https://gist.github.com/KurtJacobson/34a2e45ea2227ba58702fc1cb0372c40

you must copy that proper version of libpyqt5_py2.so to the folder:
/usr/lib/x86_64-linux-gnu/qt5/plugins/designer
(x86_64-linux-gnu might be called something slightly different 
on different systems)

You will require super user privileges to copy the file to the folder.

then you must link the qtvcp_plugin.py to the folder that designer will search.

This can be:
/usr/lib/x86_64-linux-gnu/qt5/plugins/designer/python
or 
~/.designer/plugins/python

open a terminal, set the environment for linuxcnc (. scripts/rip-environment)
then load designer with : designer -qt=5


