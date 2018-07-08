#!/usr/bin/env python
import hal,time,os,sys
import traceback
from configobj import ConfigObj
from pyui import widgets as WIDGETS
from pyui import commands

# path to the configuration the user requested
try:
    CONFIGPATH = os.environ['CONFIG_DIR']
    CONFIGDIR = os.path.join(CONFIGPATH, '_panelui.ini')
except:
    try:
        CONFIGDIR = os.path.abspath(os.path.join(os.path.dirname( '..','_panelui.ini')))
    except:
        try:
            CONFIGPATH = os.path.expanduser("~")
            CONFIGDIR = os.path.join(CONFIGPATH, '_panelui.ini')
        except:
            print 'PANELUI ERROR: No _panelui.ini found.'
            sys.exit(0)

P='Pressed'
R='Released'
DBG_state = 0
def DBG(str,level=1):
    if DBG_state >= level:
        print str

class keyboard():
    def __init__(self, filename=CONFIGDIR):
        self.filename = filename

    def validate(self):
        from pyui import panelui_validate
        sys.exit(0)

    def build(self,dbg_state=0):
        global DBG_state
        DBG_state = self._dbg = dbg_state
        def build_widget(widget,array,idname):
            # get attributes of the widget
            metadata={}
            for attribute in array:
                value = array[attribute]
                DBG( '      >> %s = %s'%(attribute,value),2)
                # if attribute is a list, make it a single list
                # the widgets.command method will decode that list later
                if isinstance(value,list) and len(value) > 2:
                    temp=[];temp2=[]
                    temp.append(value[0])
                    for i in range(1, len(value)):
                        temp2.append(value[i])
                    temp.append(temp2)
                    value = temp
                # key is how we sort the dict of commands
                # by the unigue key position or group name
                if attribute.upper() == 'KEY':
                    self.r_c[value] = idname
                metadata[attribute.upper()] = value
            # intialize the widget
            widget.hal_init(self.hal, idname, metadata, 
                    self.cmd, self.widgets, DBG_state)
            self.widgets[idname] = widget

        self.hal = hal.component('_panel_ui_py_')
        self.widgets = {}
        self.r_c = {}
        self.config = ConfigObj(self.filename)
        try:
            self.hal.setprefix(self.config['HAL_PREFIX']['NAME'])
        except:
            self.hal.setprefix('panelui')
        self.cmd = commands.CNC_COMMANDS(self)
        for basewidget in self.config:
            if basewidget == 'HAL_PREFIX': continue
            DBG('\n List of %s:'% (basewidget),2 )
            if basewidget in dir(WIDGETS):
                for idname in self.config[basewidget]:
                    DBG( '  > %s'%(idname),2)
                    # instantinate the widget
                    if basewidget =='TOGGLE_BUTTONS':
                        widget = WIDGETS.TOGGLE_BUTTONS()
                        build_widget(widget,self.config[basewidget][idname],idname)
                    elif basewidget =='MOMENTARY_BUTTONS':
                        widget = WIDGETS.MOMENTARY_BUTTONS()
                        build_widget(widget,self.config[basewidget][idname],idname)
                    elif basewidget =='RADIO_BUTTONS':
                        # look for groups
                        group_list=[]
                        for group in self.config[basewidget][idname]:
                            value = self.config[basewidget][idname][group]
                            if not isinstance(value,dict): continue
                            group_list.append(group)
                            DBG( 'GROUP: %s'% (group),2 )
                            widget2 = WIDGETS.RADIO_BUTTONS()
                            build_widget(widget2,self.config[basewidget][idname][group],group)
                        DBG( group_list,2 )
                        widget = WIDGETS.GROUP()
                        widget.add_list(group_list)
                        build_widget(widget,self.config[basewidget][idname],idname)
                            
                    else:
                        continue
            else:
                print 'PANELUI INFO: %s not defined'% basewidget

        self.hal.ready()
        for k in self.r_c:
            DBG('Keycode: %s <-> Widget: %s'%( k,self.r_c[k]),1)

    def update(self, *arg):
        raw=arg[0]; row=arg[1];column=arg[2];state=arg[3]
        #print 'raw',raw,'row:',row,'column:',column,'state:',state
        if raw == 0: return
        button = 'R%dC%d'%(row,column)
        try:
            DBG('%s %s %s'% ( button, self.r_c[button], P if state else R ),1)
            self.widgets[self.r_c[button]].toggle_state(state)
        except  Exception, e:
            if DBG_state >1:
                exc_type, exc_value, exc_traceback = sys.exc_info()
                formatted_lines = traceback.format_exc().splitlines()
                print
                print "****PYUI verbose debugging:",formatted_lines[0]
                traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
                print formatted_lines[-1]
            elif DBG_state >0:
                print 'PYUI ERROR: no button or command assigned to keycode %s?'% button
                print e
            else:
                pass

    def periodic(self):
        try:
            self.cmd.handler_instance.periodic()
        except:
            self.cmd.periodic()

    def exit(self):
        print 'Python exit'
        self.hal.exit()

    def __getitem__(self, item):
        return self.widgets[item]
    def __setitem__(self, item, value):
        self.widgets[item] = value


def main():
    print 'main'
    key=keyboard()
    a=key.build()
    b=key.loop()

if __name__ == "__main__":
    main()

