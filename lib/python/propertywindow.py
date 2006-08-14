import Tkinter

def properties(app, title, names, props):
    t = Tkinter.Toplevel(app)
    t.wm_transient(app)
    t.wm_title(title)
    t.wm_iconname(title)
    t.wm_resizable(0,0)
     
    for i, (k, n) in enumerate(names):
	if k not in props: continue
        l = Tkinter.Label(t, text = n)
        l.grid(row=i, column=0, sticky="nw")
        l = Tkinter.Label(t, text = str(props[k]), wraplength=200, justify="l")
        l.grid(row=i, column=1, sticky="nw")
    
    b = Tkinter.Button(t, text=_("OK"), command=t.destroy, default="active",
		padx=0, pady=0, width=10)
    b.grid(row=i+1, column=0, columnspan=2) 
    t.after_idle(b.focus)
    t.bind("<Escape>", lambda e: t.destroy()) 
    t.bind("<Return>", lambda e: t.destroy()) 
    return t


