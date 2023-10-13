Converting vismach to qt_vimach:
The code lines may look a little different in
each case, but the premise is the same.



#######################################################
Qt uses different imported libraries
#######################################################

change:
from vismach import *

to:
from qtvcp.lib.qt_vismach.qt_vismach import *



#######################################################
Invoking script directly from python3 requires changes.
#######################################################

change:
main(model, tooltip, work, size=1500,lat=-65, lon=45)

to:
if __name__ == '__main__':
    main(model, tooltip, work, size=1500,lat=-65, lon=45)


####################################################################
Invoking the script from Qtvcp requires a window object to be added.
#####################################################################
Add: (usually just above 'if __name__ == '__main__':')


# we want to embed with qtvcp so build a window to display
# the model
class Window(QWidget):

    def __init__(self):
        super(Window, self).__init__()
        self.glWidget = GLWidget()
        v = self.glWidget
        v.set_latitudelimits(-180, 180)

        # remove HUD cde if no HUD
        v.hud = myhud
        # HUD needs to know where to draw
        v.hud.app = v

        world = Capture()

        v.model = Collection([model, world])
        size = 600
        v.distance = size * 3
        v.near = size * 0.01
        v.far = size * 10.0
        v.tool2view = tooltip
        v.world2view = world
        v.work2view = work

        mainLayout = QHBoxLayout()
        mainLayout.addWidget(self.glWidget)
        self.setLayout(mainLayout)

#################################################################
might also want to look at removing the component pins and read
HAL pins directly. Nice not to have to connect pins for the model
to work.
##################################################################
