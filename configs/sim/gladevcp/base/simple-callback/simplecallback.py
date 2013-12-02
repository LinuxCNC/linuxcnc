
nhits = 0

def on_button_press(gtkobj,data=None):
    global nhits
    nhits += 1
    print "on_button_press callback"
    gtkobj.set_label("hits: %d" % nhits)
