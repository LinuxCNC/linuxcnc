nhits = 0
def on_button_press(widget,data=None):
    global nhits 
    nhits += 1 
    widget.set_label("hits: %d" % nhits)
