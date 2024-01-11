#!/usr/bin/python3
# scratch py to see if spreadshgett vbar and hnar value are ok
# 21oct2023 by using a file for the 'thumb' of the pyvcp_widget call bar 
# the user can make any shape move alonmg the bar's 'rail'(aka elevator)
# Imagine a musioc clef drawing for volume
# and a faucet pourinbf water
# or the word "Hz" for frequency
#    i had to cjeclk. was that Hz or hZ or hz... ok Big H little z
#
from tkinter import *

#def drawMarker(canvas):
def createMarker(self,cw,bh,max_,min_,value):
    # 22oct what shape  maltese cross?
    mx = cw/2 # ng canvas has no w
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
    #22oct iwhy is the shape filled? its dark
    self.itemconfig(self.marker,fill="red")
