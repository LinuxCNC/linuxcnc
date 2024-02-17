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
#17nov i padd self, so all data is supplied, dont pass parts of self
def createMarker(self):
    print("in createMarker vbarRect-1.py")
    #
    mx = int(self.cw/2)
    #
    range = (self.max_ - self.min_)/2
    my = int((self.value/range)*range)
    my=my-self.pxPadVert
    #
    qw = int(mx/2) # for horz make sure marker is taller than wide
    #
    #print("in createMarker value = ",self.value," mx = ", mx, " my = ", my, " qw = ", qw)
    #
    self.marker = self.create_polygon(
        qw   ,  my+2,
        mx+qw,  my+2,
        mx+qw,  my-2,
        qw   ,  my-2,
        qw   ,  my+2,
        width=1        
    )
    #22oct iwhy is the shape filled? its dark
    # well red works, so the dark color is some default of i set it  dunno how where
    # buty i see the thumb is BEHIND the raiul!
    self.itemconfig(self.marker,fill="red")
