# user command file to increase default axis gui size

# invoke with the ini file item:
# [DISPLAY]USER_COMMAND_FILE = axis_resize.tcl

maxgeo=root_window.tk.call("wm","maxsize",".")
fullw=maxgeo.split(' ')[0]
fullh=maxgeo.split(' ')[1]
fullsize=fullw +'x' + fullh

# custom size (to accomodate bigger gladevcp panels)
newsize = '800' + 'x' + '680'
# for fullsize: uncomment this
# newsize=fullsize

root_window.tk.call("wm","geometry",".",newsize)

