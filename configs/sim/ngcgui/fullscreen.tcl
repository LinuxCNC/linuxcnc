# Expand to fullscreen
# for axis gui, include ini item:
# [DISPLAY]USER_COMMAND_FILE = fullscreen.tcl
maxgeo=root_window.tk.call("wm","maxsize",".")
fullsize=maxgeo.split(' ')[0] + 'x' + maxgeo.split(' ')[1]
root_window.tk.call("wm","geometry",".",fullsize)
