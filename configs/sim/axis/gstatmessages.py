
from hal_glib import GStat
from common.iniinfo import _IStat as IStatParent

class Info(IStatParent):
    _instance = None
    _instanceNum = 0

    def __new__(cls, *args, **kwargs):
        if not cls._instance:
            cls._instance = IStatParent.__new__(cls, *args, **kwargs)
        return cls._instance

INFO = Info()

GSTAT = GStat()
GSTAT.forced_update()
GSTAT.connect('jograte-changed', lambda w, data: vars.jog_speed.set(data))
GSTAT.connect('axis-selection-changed', lambda w,data: select_axis(data))
GSTAT.connect('cycle-start-request', lambda w, state : cycle_start_request(state))
GSTAT.connect('cycle-pause-request', lambda w, state: pause_request(state))
GSTAT.connect('ok-request', lambda w, state: dialog_ext_control(w,1,1))
GSTAT.connect('cancel-request', lambda w, state: dialog_ext_control(w,1,0))
GSTAT.connect('macro-call-request', lambda w, name: request_macro_call(name))
GSTAT.connect('softkey-pressed', lambda w,data: softkey_pressed(data))
GSTAT.connect('shutdown-request', lambda w : General_Halt())
GSTAT.connect('reload-display', lambda w : commands.clear_live_plot())

global last_mpg
last_mpg = 0
mpg_enabled = 0


def user_hal_pins():
    comp.newpin('mpg-enable', hal.HAL_BIT, hal.HAL_IN)
    comp.newpin('mpg-in', hal.HAL_S32, hal.HAL_IN)
    comp.ready()

def user_live_update():
    GSTAT.run_iteration()
    global mpg_enabled
    try:
        if comp['mpg-enable'] or mpg_enabled: 
            global last_mpg
            if comp['mpg-in'] == last_mpg: return
            if comp['mpg-in'] > last_mpg:scroll_up(None)
            if comp['mpg-in'] < last_mpg:scroll_down(None)
            last_mpg = comp['mpg-in']
    except Exception as e:
        print(e)

def select_axis(data):
    global mpg_enabled
    if data is None: return
    if data =='MPG0':
        mpg_enabled = True
        return
    mpg_enabled = False
    if data.upper() =='NONE':
        return
    try:
        widget = getattr(widgets, "axis_%s" % data.lower())
        widget.focus()
        widget.invoke()
    except:
        pass

def cycle_start_request(state):
    print('cycle start',state)
    commands.task_run(None)

def pause_request(state):
    print('cycle pause',state)
    commands.task_pauseresume(None)

def dialog_ext_control(widget,t,state):
    print('dialog control',widget,state)

    flag = False
    for child in root_window.winfo_children():
        #print(child)
        if isinstance(child, Tkinter.Toplevel):
            #print(f"Found a Toplevel window: {child}")
            if '.!toplevel' in str(child):
                #print('sending command:',child)
                for child2 in child.winfo_children():
                    #print(child2)
                    if isinstance(child2, Tkinter.Frame):
                        for child3 in child2.winfo_children():
                            #print(child3)
                            if isinstance(child3, Tkinter.Button):
                                #print(dir(child3))
                                txt = child3.cget("text")
                                if txt.lower() == 'ok' and state:
                                    #print('Ok')
                                    child3.invoke()
                                    flag = True
                                    break
                                elif txt.lower() == 'cancel' and not state:
                                    #print('Cancel')
                                    child3.invoke()
                                    flag = True
                                    break
                    if flag: break
            if flag: break
    else:
        #print('No window')
        # remove one error message
        if state == 0:
            notifications.clear_one()

def request_macro_call(name):
        #print('request macro:',name)
        cmd = INFO.get_ini_mdi_command(name)
        #print(f'MDI command:{cmd}   name:{name}')
        if not INFO.get_ini_mdi_command(name) is None:
            run_mdi(data=cmd)
        else:
            #print(INFO.get_ini_macro_command(name))
            try:
                temp = INFO.MACRO_COMMAND_DICT.get(name).get('cmd')
                #print(temp)
                run_macro(data=temp)
            except Exception as e:
                print(e)

def run_mdi(data):
    #print(f'run mdi command:{data}')
    mdi_list = data.split(';')
    for code in (mdi_list):
        commands.send_mdi_command(code)

def run_macro(data):
    #print(f'run macro:{data}')
    o_codes = data.split()
    command = str( "O<" + o_codes[0] + "> call" )
    # check for oword and confirm path exists
    #if not self.check_macro_path(command):
    #    return
    for code in o_codes[1:]:
        if vars.metric.get(): unit_str = " " + _("mm")
        else: unit_str = " " + _("in")
        param = prompt_float("Macro", f"Enter a value for: {code}:",
                "", unit_str)
        if param <= 0: return
        if vars.metric.get(): param /= 25.4
        command = command + " [" + str(param) + "] "
    commands.send_mdi_command(command)

def softkey_pressed(index):

        if index == 0:
            root_window.tk.call('.pane.top.tabs','raise','manual')
        elif index == 1:
            root_window.tk.call('.pane.top.tabs','raise','mdi')
        elif index == 2:
            root_window.tk.call('.pane.top.right','raise','preview')
        elif index == 3:
            root_window.tk.call('.pane.top.right','raise','numbers')
        elif index == 4:
            root_window.tk.call('.pane.top.right','raise','user_0')
        else:
            print(f'Softkey index:{index}')
