import xml.dom.minidom
import xml.dom.ext
from Tkinter import *
import sys, os
import emc
from pyvcp import *
from hal import *; import time


filename="testfile.xml"



def read_file():
     doc = xml.dom.minidom.parse(filename) 
     # find the pydoc element
     for e in doc.childNodes:
          if e.nodeType == e.ELEMENT_NODE and e.localName == "pyvcp":
               print "found pyvcp element"
               break
     if e.localName != "pyvcp":
          print "Error: no pyvcp element in file!"
     pyvcproot=e
     print "PYVCP contains:"
     level=0
     nodeiterator(pyvcproot,pyvcp0,level)          
     #for e in pyvcproot.childNodes:

     print "-------------------------"
     print "file read"
     # print doc.childNodes
     # pass


num=0
def nodeiterator(node,widgetparent,level):
     els =["led","vbox","hbox","vbox"
          ,"button","scale","checkbutton","bar","label","number"]
     global num
     num+=1
     params=[]
     for e in node.childNodes:
          if e.nodeType == e.ELEMENT_NODE and (e.nodeName in els):  
               params = paramiterator(e)  # find all the parameters for this node
               parent = str(e.parentNode.nodeName) + str(level)
               newwidget = widget_creator(widgetparent,e.nodeName,params,level+1)
               nodeiterator(e,newwidget,level+1)
      



def widget_creator(parent,widget_name,params,level):
  
     global widgets
     widgethandle= str(widget_name) + str(level)
   
     w_command=""
     w_command += "pyvcp_"+str(widget_name)+"("+"parent"+",pycomp"
     for a in params:
          w_command=w_command+","+str(a) 
     w_command=w_command+")"
     
     # this prints the statement generated above, 
     print str(widgethandle) + " = " + w_command
     # and evaluated here:
     locals()[widgethandle] = eval(w_command)

     # pack the widget according to parent
     # either hbox or vbox
     if parent==pyvcp0:
          #print "this is the root element"
          pack_command = widgethandle +".pack(side='top',fill=X)"
     else:
          a= parent.packtype()
          if a == "top":
               pack_command = widgethandle +".pack(side='top',fill=X)"
          else:
               pack_command = widgethandle +".pack(side='left',fill=Y)"
               
          
     print pack_command
     eval(pack_command)

     # add the widget to a global list widgets
     # to enable calling update() later
     list_command = 'widgets.append( ' + widgethandle + ' ) '
     eval(list_command)
     return_command = "return "+str(widgethandle) 
     
 
     return eval(str(widgethandle))



def paramiterator(node):
     " this function returns a list of all parameters for a widget element"

     params = ["size","text","orient","halpin","format","font"]
     outparams =[]
     for e in node.childNodes:
          if e.nodeType == e.ELEMENT_NODE and (e.nodeName in params):
               outparams.append(str(e.nodeName) + \
                    " = " +  str(e.childNodes[0].nodeValue) )
     return outparams



def updater():
     " this function goes through all widgets and calls update() on them "
     global widgets, pycomp
     for a in widgets:
          a.update(pycomp)
     pyvcp0.after(100,updater)

widgets=[];

def create_vcp(master, comp = None):
    global pyvcp0, pycomp
    pyvcp0 = master
    if comp is None: comp = component("pyvcp")
    pycomp = comp
    print pycomp
    read_file()
    updater()
    
if __name__ == '__main__':
    pyvcp0 = Tk()
    create_vcp(pyvcp0)
    pyvcp0.mainloop()


