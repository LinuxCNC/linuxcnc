# Test for movement after abort

Test for [issue #865][1], where an abort while interp is loading a
long `.ngc` file, and a block restoring a state tag triggers a
readahead bug where unflushed segments are appended to the interp
list.

[1]:  https://github.com/LinuxCNC/linuxcnc/issues/865
