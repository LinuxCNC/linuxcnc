
from hal_glib import GStat
GSTAT = GStat()
GSTAT.forced_update()
GSTAT.connect('jograte-changed', lambda w, data: vars.jog_speed.set(data))
GSTAT.connect('axis-selection-changed', lambda w,data: select_axis(data))
GSTAT.connect('cycle-start-request', lambda w, state : cycle_start_request(state))
GSTAT.connect('cycle-pause-request', lambda w, state: pause_request(state))
GSTAT.connect('ok-request', lambda w, state: dialog_ext_control(w,1,1))
GSTAT.connect('cancel-request', lambda w, state: dialog_ext_control(w,1,0))
GSTAT.connect('macro-call-request', lambda w, name: request_macro_call(name))
def user_live_update():
    GSTAT.run_iteration()

def select_axis(data):
    if data is None: return
    widget = getattr(widgets, "axis_%s" % data.lower())
    widget.focus()
    widget.invoke()

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
    print('request macro:',name)
