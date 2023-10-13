# Test for movement after abort

Test for issue [#579][1], [#393][2] & [#241][3], where an abort while
interp is loading a long `.ngc` file, and an `ON_ABORT_COMMAND` block
triggers a readahead bug where unflushed segments are appended to the
interp list.

[1]:  https://github.com/LinuxCNC/linuxcnc/issues/579
[2]:  https://github.com/LinuxCNC/linuxcnc/issues/393
[3]:  https://github.com/LinuxCNC/linuxcnc/issues/241
