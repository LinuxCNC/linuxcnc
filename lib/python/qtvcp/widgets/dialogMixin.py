from PyQt5.QtWidgets import (QDesktopWidget, QLabel,QMessageBox)
from qtvcp.widgets.widget_baseclass import _HalWidgetBase
from qtvcp import logger

LOG = logger.getLogger(__name__)
# Force the log level for this module
LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

    #########################################
    # geometry helper functions
    #########################################

    # This general function parses the geometry string and places
    # the dialog based on what it finds.
    # there are directive words allowed.
    # If there are no letters in the string, it will check the
    # preference file (if there is one) to see what the last position
    # was. If all else fails it uses it's natural Designer stated
    # geometry
class GeometryMixin(_HalWidgetBase):
    def __init__(self, ):
        super(GeometryMixin, self).__init__()
        self._geometry_string = 'default'
        self._default_geometry = None

    def get_default_geometry(self):
        a,b,c,d = self._default_geometry
        return '%s %s %s %s'% (a,b,c,d)

    def set_default_geometry(self):
        geom = self.frameGeometry()
        geom.moveCenter(QDesktopWidget().availableGeometry().center())
        self.setGeometry(geom)
        x = self.geometry().x()
        y = self.geometry().y()
        w = 300 #w = self.geometry().width()
        h = 150 #h = self.geometry().height()
        self._default_geometry=[x,y,w,h]
        return x,y,w,h

    # only valid is dialog has been shown
    def get_current_geometry(self):
        x = self.geometry().x()
        y = self.geometry().y()
        w = self.geometry().width()
        h = self.geometry().height()
        return '%s %s %s %s'% (x,y,w,h)

    def read_preference_geometry(self,name):
        self._geoName = name
        if self.PREFS_:
            self._geometry_string = self.PREFS_.getpref(name,
                                        self._geometry_string,
                                        str, 'DIALOG_GEOMETRY')

    def set_geometry(self):
        #print(self.objectName(),'set geo',self._geometry_string)
        try:
            if self._geometry_string.replace(' ','').isdigit() and self.PREFS_:
                # If there is a preference file object use it to load the geometry
                self._geometry_string = self.PREFS_.getpref(self._geoName, '', str, 'DIALOG_GEOMETRY')

            # use the previously calculated default.
            if self._geometry_string in('default',''):
                x,y,w,h = self._default_geometry
                self.setGeometry(x,y,w,h)
                if not 'always' in self._geometry_string.lower():
                    self._geometry_string = self.get_current_geometry()    
             
            # center of desktop
            # add 'always' or the user can reset the dialog
            elif 'center' in self._geometry_string.lower():
                geom = self.frameGeometry()
                geom.moveCenter(QDesktopWidget().availableGeometry().center())
                self.setGeometry(geom)
                if not 'always' in self._geometry_string.lower():
                    self._geometry_string = self.get_current_geometry()

            # bottom left of desktop
            # add 'always' or the user can reset the dialog
            elif 'bottomleft' in self._geometry_string.lower():
                # move to bottom left of parent
                ph = QDesktopWidget().geometry().height()
                px = QDesktopWidget().geometry().x()
                py = QDesktopWidget().geometry().y()
                dw = self.geometry().width()
                dh = self.geometry().height()
                self.setGeometry(px, py+ph-dh, dw, dh)
                if not 'always' in self._geometry_string.lower():
                    self._geometry_string = self.get_current_geometry()

            # to be always on (relative to) parent but as assigned size
            # ie: Dialog-geometry = onwindow 100 100 280 118
            # add 'always' or the user can reset the dialog
            elif 'onwindow' in self._geometry_string.lower():
                # move relative to parent position
                px = self.QTVCP_INSTANCE_.geometry().x()
                py = self.QTVCP_INSTANCE_.geometry().y()
                # remove everything except digits and spaces
                temp=''
                for x in self._geometry_string:
                    if (x.isdigit() or x == ' '):
                        temp = temp+x
                # remove lead and trailing spaces and then split on spaces
                temp = temp.strip(' ').split(' ')
                self.setGeometry(px+int(temp[0]), py+int(temp[1]), int(temp[2]), int(temp[3]))
                if not 'always' in self._geometry_string.lower():
                    self._geometry_string = self.get_current_geometry()

            # half the main window height/width
            # add 'always' or the user can reset the dialog
            elif 'half' in self._geometry_string.lower():
                h = int(self.QTVCP_INSTANCE_.geometry().height() /2)
                w = int(self.QTVCP_INSTANCE_.geometry().width() /2)

                x = int(self.geometry().x())
                y = int(self.geometry().y())
                self.setGeometry( x,y,w,h)

                if not 'always' in self._geometry_string.lower():
                    self._geometry_string = self.get_current_geometry()

            else:
                # assuming geometry is actual size/position
                temp = self._geometry_string.split(' ')
                self.setGeometry(int(temp[0]), int(temp[1]), int(temp[2]), int(temp[3]))
                LOG.debug('Setting {} dialog geometry from {} from prefs.'.format( self._geoName,temp))
        except Exception as e:
            print(e)
            try:
                LOG.error('Calculating geometry of {} widget using: {}. Will use default placement.'.format(self.HAL_NAME_, self._geometry_string))
            except AttributeError as f:
                print(f)
            LOG.debug('Dialog geometry python error: {}'.format(e))
            x = self.geometry().x()
            y = self.geometry().y()
            self.setGeometry( x,y,300,150)
            self._geometry_string = 'default'

    def record_geometry(self):
        # message box are difficult
        if isinstance(self,QMessageBox):
            if self._geometry_string.replace(' ','').isdigit():
                if self.PREFS_ :
                    geometry_string = self.PREFS_.getpref(self._geoName, '', str, 'DIALOG_GEOMETRY')
                else:
                    geometry_string = ''
                temp = self._geometry_string.split(' ')
                #print('record:',temp,self.geometry())
                x = self.geometry().x()
                y = self.geometry().y()
                w = int(temp[2])
                h = self.geometry().height()
                geo = '%s %s %s %s'% (x,y,w,h)
                if self.PREFS_ :
                    self.PREFS_.putpref(self._geoName, geo, str, 'DIALOG_GEOMETRY')
                return
        try:
            if self.PREFS_ :
                temp = self._geometry_string.replace(' ','')
                temp = temp.strip('-')
                if temp in('','default')  or temp.isdigit():
                    LOG.debug('Saving {} data from widget {} to file.'.format( self._geoName,self.HAL_NAME_))
                    x = self.geometry().x()
                    y = self.geometry().y()
                    w = self.geometry().width()
                    h = self.geometry().height()
                    geo = '%s %s %s %s'% (x,y,w,h)
                    self.PREFS_.putpref(self._geoName, geo, str, 'DIALOG_GEOMETRY')
            elif not 'always' in self._geometry_string.lower():
                self._geometry_string = self.get_current_geometry()
        except Exception as e:
            print(e)
            pass


