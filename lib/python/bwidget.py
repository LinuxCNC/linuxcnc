#    Copyright 2004, 2005, 2006 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

"""Wrapper for BWidget family of widgets"""

__author__ = "Jeff Epler <jepler AT unpy DOT net>"

__all__ = """
    Entry Label Button ArrowButton ProgressBar ScrollView Separator

    MainFrame LabelFrame TitleFrame PanelFrame ScrolledWindow ScrollableFrame
    PanedWindow ButtonBox PagesManager NoteBook Dialog StatusBar

    LabelEntry ComboBox SpinBox Tree ListBox MessageDialog ProgressDialog
    PasswordDialog SelectFont SelectColor SelectColorMenu

    CASCADE CHECKBUTTON COMMAND RADIOBUTTON SEPARATOR STATUS PROGRESSION
""".split()

import Tkinter, types

def returnswidget(f):
    def w(self, *args, **kw):
        r = f(self, *args, **kw)
        return self.nametowidget(str(r))
    w.__doc__ = f.__doc__
    return w

def makeswidget(f, t):
    def w(self, *args, **kw):
        r = str(f(self, *args, **kw))
        try:
            return self.nametowidget(r)
        except KeyError:
            return makewidget(self, t, str(r))
    w.__doc__ = f.__doc__
    return w

def nametowidget(self, name):
    """Return the Tkinter instance of a widget identified by
    its Tcl name NAME."""
    w = self
    if name[0] == '.':
        w = w._root()
        name = name[1:]
    while name:
        i = name.find('.')
        if i >= 0:
            name, tail = name[:i], name[i+1:]
        else:
            tail = ''
        while tail:
            try:
                w.children[name]
            except KeyError:
                j = tail.find('.')
                if j >= 0:
                    name, tail = name + "." + tail[:j], tail[j+1:]
                else:
                    name, tail = name + "." + tail, ''
            else:
                break
        w = w.children[name]
        name = tail
    return w

Tkinter.Misc.nametowidget = nametowidget
    
def makewidget(master, klass, path):
    path = str(path)
    self = types.InstanceType(klass)
    self._name = path[len(master._w)+1:]
    self._w = path
    self.children = {}
    master.children[self._name] = self
    self.master = master
    self.tk = master.tk
    return self

class BWidget:
    def _require(self, master):
        master.tk.call("package", "require", "BWidget")
    def __init__(self, master, cnf={}, **kw):
        self._require(master)
        Tkinter.Widget.__init__(self, master, self.__class__.__name__, cnf, kw)

# Simple Widgets
class Entry(BWidget, Tkinter.Entry):
    def invoke(self):
        return self.tk.call(self._w, "invoke")

class Label(BWidget, Tkinter.Label):
    def setfocus(self):
        return self.tk.call(self._w, "setfocus")

class Button(BWidget, Tkinter.Button): pass

class ArrowButton(BWidget, Tkinter.Button): pass

class ProgressBar(BWidget, Tkinter.Widget): pass

class ScrollView(BWidget, Tkinter.Widget): pass

class Separator(BWidget, Tkinter.Widget): pass
    
# Manager Widgets

class _Frame:
    def getframe(self):
        return self.tk.call(self._w, "getframe")
    getframe = makeswidget(getframe, Tkinter.Frame)

class _Items:
    def itemcget(self, index, option):
        return self.tk.call(self._w, "itemcget", index, '-' + option)

    def itemconfigure(self, index, cnf=None, **kw):
        return self._configure(('itemconfigure', index), cnf, kw) 

CASCADE="cascade"
CHECKBUTTON="checkbutton"
COMMAND="command"
RADIOBUTTON="radiobutton"
SEPARATOR="separator"
STATUS = "status"
PROGRESSION = "progression"

class MainFrame(BWidget, _Frame, Tkinter.Widget):
    def addindicator(self, **kw):
        return self.tk.call(self._w, "addindicator", *self._options(kw))
    addindicator = makeswidget(addindicator, Label)

    def getindicator(self, i):
        return self.tk.call(self._w, "getindicator", i)
    getindicator = returnswidget(getindicator)

    def getmenu(self):
        return self.tk.call(self._w, "getmenu")
    getmenu = returnswidget(getmenu)

    def setmenustate(self, tag, state):
        return self.tk.call(self._w, "setmenustate", tag, state)

    def showstatusbar(self, name):
        return self.tk.call(self._w, "showstatusbar", name)

    def showtoolbar(self, index, bool_):
        return self.tk.call(self._w, "showtoolbar", index, bool_)

class LabelFrame(BWidget, _Frame, Tkinter.Widget):
    def align(self, others):
        return self.tk.call("LabelFrame::align", self, *others)

class TitleFrame(BWidget, _Frame, Tkinter.Frame): pass

class PanelFrame(BWidget, Tkinter.Frame): pass

class ScrolledWindow(BWidget, _Frame, Tkinter.Frame):
    def setwidget(self, child):
        return self.tk.call(self._w, "setwidget", child)

class ScrollableFrame(BWidget, _Frame, Tkinter.Frame):
    def see(self, w, vert=None, horiz=None):
        if vert is None and horiz is None:
            return self.tk.call(self._w, "see", w)
        return self.tk.call(self._w, "see", w, vert, horiz)

    def xview(self, *args):
        return self.tk.call(self._w, "xview", *args)

    def yview(self, *args):
        return self.tk.call(self._w, "yview", *args)

class PanedWindow(BWidget, Tkinter.Frame):
    def add(self, **kw):
        return self.tk.call(self._w, "add", *self._options(kw))
    add = makeswidget(add, Tkinter.Frame)

    def getframe(self, index):
        return self.tk.call(self._w, "getframe", index)
    getframe = makeswidget(getframe, Tkinter.Frame)

class ButtonBox(BWidget, _Items, Tkinter.Frame):
    def add(self, **kw):
        return self.tk.call(self._w, "add", *self._options(kw))
    add = makeswidget(add, Button)

    def delete(self, index):
        self.tk.call(self._w, "delete", index)

    def index(self, item):
        self.tk.call(self._w, "index", item)

    def insert(self, index, *kw):
        return self.tk.call(self._w, "insert", index, *self._options(kw))
    insert = makeswidget(insert, Button)

    def invoke(self, index):
        return self.tk.call(self._w, "invoke", index)

    def setfocus(self, index):
        return self.tk.call(self._w, "setfocus", index)

class PagesManager(BWidget, Tkinter.Frame):
    def add(self, page):
        return self.tk.call(self._w, "add", page)
    add = makeswidget(add, Tkinter.Frame)

    def compute_size(self):
        return self.tk.call(self._w, "compute_size")

    def delete(self, page):
        return self.tk.call(self._w, "delete", page)

    def getframe(self, page):
        return self.tk.call(self._w, "getframe", page)
    getframe = makeswidget(getframe, Tkinter.Frame)

    def pages(self, *args):
        return self.tk.call(self._w, "pages", *args)

    def raise_page(self, page=None):
        if page is None:
            return self.tk.call(self._w, "raise")
        return self.tk.call(self._w, "raise", page)

class NoteBook(PagesManager, _Items):
    def bindtabs(self, event, script):
        return self.tk.call(self._w, "bindtabs", event, script)

    def delete(self, page, destroyframe=True):
        return self.tk.call(self._w, "delete", page, destroyframe)

    def insert(self, index, page, **kw):
        return self.tk.call(self._w, "insert", index, page, *self._options(kw))
    insert = makeswidget(insert, Tkinter.Frame)

    def move(self, page, index):
        return self.tk.call(self._w, "move", page, index)

    def see(self, page):
        return self.tk.call(self._w, "see", page)

    def raise_page(self, page=None):
        if page is None:
            return self.tk.call(self._w, "raise")
        else:
            return self.tk.call(self._w, "raise", page)

class Dialog(ButtonBox, Tkinter.BaseWidget, _Frame):
    def draw(self, focus=None):
        if focus is None:
            return self.tk.call(self, "draw")
        return self.tk.call(self._w, "draw", focus)

    def enddialog(self):
        return self.tk.call(self._w, "enddialog")

    def withdraw(self):
        return self.tk.call(self._w, "withdraw")

class StatusBar(BWidget): pass

class LabelEntry(Entry): pass
    
class ComboBox(Entry):
    def bind_entry(self, *args):
        return self.tk.call(self._w, "bind", *args)

    def getlistbox(self):
        r = str(self.tk.call(self._w, "getlistbox"))
        try:
            return self.nametowidget(r)
        except KeyError:
            c = self.tk.call("winfo", "class", r)
            if c == "ListBox":
                return makewidget(self, ListBox, r)
            else:
                return makewidget(self, Tkinter.Listbox, r)

    def getvalue(self):
        return self.tk.call(self._w, "getvalue")

    def post(self):
        return self.tk.call(self._w, "post")

    def setvalue(self, index):
        return self.tk.call(self._w, "setvalue", index)

    def unpost(self):
        return self.tk.call(self._w, "unpost")

class SpinBox(Entry):
    def bind_entry(self, *args):
        return self.tk.call(self._w, "bind", *args)

    def setvalue(self, index):
        return self.tk.call(self._w, "setvalue", index)

    def getvalue(self):
        return self.tk.call(self._w, "getvalue")

class Tree(BWidget, Tkinter.Widget, _Items):
    def bind_image(self, event, script):
        return self.tk.call(self._w, "bindImage", event, script)

    def bind_text(self, event, script):
        return self.tk.call(self._w, "bindText", event, script)

    def closetree(self, node):
        return self.tk.call(self._w, "closetree", node)

    def delete(self, arg, *args):
        return self.tk.call(self._w, "delete", arg, *args)

    def edit(self, node, text, *args):
        "edit(self, node, text, verifycmd=None, clickres=None, select=None)"
        return self.tk.call(self._w, "edit", node, text, *args)

    def exists(self, node):
        return self.tk.call(self._w, "exists", node)

    def index(self, node):
        return self.tk.call(self._w, "index", node)

    def insert(self, index, parent, node="#auto", **kw):
        return self.tk.call(self._w, "insert", index, parent, node,
                *self._options(kw))

    def move(self, parent, node, index):
        return self.tk.call(self._w, "move", parent, node, index)

    def nodes(self, node, *args):
        return self.tk.call(self._w, "nodes", *args)

    def opentree(self, node, recurse=True):
        return self.tk.call(self._w, "opentree", node, recurse)

    def parent(self, node):
        return self.tk.call(self._w, "parent", node)

    def reorder(self, node, neworder):
        return self.tk.call(self._w, "reorder", node, neworder)

    def see(self, node):
        return self.tk.call(self._w, "see", node)

    def selection_add(self, *args):
        return self.tk.call(self._w, "selection", "add", *args)

    def selection_clear(self):
        return self.tl.call(self._w, "selection", "clear")

    def selection_get(self):
        return self.tl.call(self._w, "selection", "get")

    def selection_includes(self, node):
        return self.tl.call(self._w, "selection", "includes")

    def selection_remove(self, *args):
        return self.tk.call(self._w, "selection", "remove", *args)

    def selection_set(self, *args):
        return self.tk.call(self._w, "selection", "set", *args)

    def selection_toggle(self, *args):
        return self.tk.call(self._w, "selection", "toggle", *args)

    def toggle(self, node):
        return self.tk.call(self._w, "toggle", node)
    
    def visible(self, node):
        return self.tk.call(self._w, "visible", node)

    def xview(self, *args):
        return self.tk.call(self.xview, *args)

    def yview(self, *args):
        return self.tk.call(self.yview, *args)

class ListBox(BWidget, Tkinter.Widget, _Items):
    def bind_image(self, event, script):
        return self.tk.call(self._w, "bindImage", event, script)

    def bind_text(self, event, script):
        return self.tk.call(self._w, "bindText", event, script)

    def delete(self, arg, *args):
        return self.tk.call(self._w, "delete", arg, *args)

    def edit(self, item, text, *args):
        "edit(self, item, text, verifycmd=None, clickres=None, select=None)"
        return self.tk.call(self._w, "edit", item, text, *args)

    def exists(self, item):
        return self.tk.call(self._w, "exists", item)

    def index(self, item):
        return self.tk.call(self._w, "index", item)

    def insert(self, index, item="#auto", **kw):
        return self.tk.call(self._w, "insert", parent, item,
                *self._options(kw))

    def items(self, item, *args):
        return self.tk.call(self._w, "items", *args)

    def move(self, parent, item, index):
        return self.tk.call(self._w, "move", parent, item, index)

    def reorder(self, node, neworder):
        return self.tk.call(self._w, "reorder", node, neworder)

    def see(self, node):
        return self.tk.call(self._w, "see", node)

    def selection_add(self, *args):
        return self.tk.call(self._w, "selection", "add", *args)

    def selection_clear(self):
        return self.tl.call(self._w, "selection", "clear")

    def selection_get(self):
        return self.tl.call(self._w, "selection", "get")

    def selection_includes(self, node):
        return self.tl.call(self._w, "selection", "includes")

    def selection_remove(self, *args):
        return self.tk.call(self._w, "selection", "remove", *args)

    def selection_set(self, *args):
        return self.tk.call(self._w, "selection", "set", *args)

    def selection_toggle(self, *args):
        return self.tk.call(self._w, "selection", "toggle", *args)

    def xview(self, *args):
        return self.tk.call(self.xview, *args)

    def yview(self, *args):
        return self.tk.call(self.yview, *args)

class MessageDialog(Dialog):
    def __init__(self, master, cnf={}, **kw):
        self._require(master)
        Tkinter.Widget.__init__(self, master, "MessageDlg", cnf, kw)
    
class ProgressDialog(Dialog):
    def __init__(self, master, cnf={}, **kw):
        self._require(master)
        Tkinter.Widget.__init__(self, master, "ProgressDlg", cnf, kw)

class PasswordDialog(Dialog):
    def __init__(self, master, cnf={}, **kw):
        self._require(master)
        Tkinter.Widget.__init__(self, master, "PasswdDlg", cnf, kw)

class SelectFont(Dialog): pass
class SelectColor(Dialog):
    def setcolor(self, index, color):
        self.tk.call("SelectColor::setcolor", index, color)

def SelectColorMenu(master, *args, **kw):
    i = id([])
    if master._w=='.':
        w = '.' + name
    else:
        w = master._w + '.' + name
    return master.tk.call("SelectColor::menu", args, *master._options(kw))

# vim:ts=8:sts=4:et:
