import gobject
import gtk
class HAL_HBox(gtk.HBox):
    __gtype_name__ = "HAL_HBox"
    def __init__(self):
        gtk.HBox.__init__(self)

class HAL_Table(gtk.Table):
    __gtype_name__ = "HAL_Table"
    def __init__(self):
        gtk.Table.__init__(self)

class HAL_ComboBox(gtk.ComboBox):
    __gtype_name__ = "HAL_ComboBox"
    def __init__(self):
        gtk.ComboBox.__init__(self)

class HAL_Button(gtk.Button):
    __gtype_name__ = "HAL_Button"
    def __init__(self):
        gtk.Button.__init__(self)

class HAL_CheckButton(gtk.CheckButton):
    __gtype_name__ = "HAL_CheckButton"
    def __init__(self):
        gtk.CheckButton.__init__(self)

class HAL_SpinButton(gtk.SpinButton):
    __gtype_name__ = "HAL_SpinButton"
    def __init__(self):
        gtk.SpinButton.__init__(self)

class HAL_RadioButton(gtk.RadioButton):
    __gtype_name__ = "HAL_RadioButton"
    def __init__(self):
        gtk.RadioButton.__init__(self)

class HAL_ToggleButton(gtk.ToggleButton):
    __gtype_name__ = "HAL_ToggleButton"
    def __init__(self):
        gtk.ToggleButton.__init__(self)

class HAL_HScale(gtk.HScale):
    __gtype_name__ = "HAL_HScale"
    def __init__(self):
        gtk.HScale.__init__(self)

class HAL_VScale(gtk.VScale):
    __gtype_name__ = "HAL_VScale"
    def __init__(self):
        gtk.VScale.__init__(self)

class HAL_ProgressBar(gtk.ProgressBar):
    __gtype_name__ = "HAL_ProgressBar"
    def __init__(self):
        gtk.ProgressBar.__init__(self)

class HAL_Label(gtk.Label):
    __gtype_name__ = "HAL_Label"
    def __init__(self):
        gtk.Label.__init__(self)
