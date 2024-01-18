#!/usr/bin/python3
# tkinter cmds to darw a rectangular ;thumnb; fir a pyvcp bar widget
# all dwgf files import the func 'createMarker'
#  but is differentiated by parent (eg  bar233.createMarker() )
from tkinter import *

def createMarker(self,cw,bh,max_,min_,value):
    mx = cw/2
    my = int(  (value / max_ )  *  bh )
    #
    self.marker = self.create_polygon(
        mx-10,  5,
        mx+10,  5,
        mx+10,  -5,
        mx-10,  -5,
        mx-10,  -5,
        width=1        
    )
    #
    self.itemconfig(self.marker,fill="red")
