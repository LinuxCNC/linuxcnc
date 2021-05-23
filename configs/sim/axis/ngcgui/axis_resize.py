# user command file to increase default axis gui size

# invoke with the ini file item:
# [DISPLAY]USER_COMMAND_FILE = axis_resize.py

maxgeo=root_window.tk.call("wm","maxsize",".")

# custom size (to accommodate bigger gladevcp panels)
newsize = '800' + 'x' + '680'
# for fullsize: uncomment this
# newsize=fullsize

root_window.tk.call("wm","geometry",".",newsize)

