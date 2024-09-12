#!/usr/bin/python3
# dwgf file for pybcp_widget class  horz bar
#
# scratch py to see if spreadshgett vbar and hnar value are ok
# 21oct2023 by using a file for the 'thumb' of the pyvcp_widget call bar 
# the user can make any shape move alonmg the bar's 'rail'(aka elevator)
# Imagine a musioc clef drawing for volume
# and a faucet pourinbf water
# or the word "Hz" for frequency
#    i had to cjeclk. was that Hz or hZ or hz... ok Big H little z
#
from tkinter import *

def createMarker(self,cw,bh,max_,min_,value):
    #
    #12nov this is only for horz bars
    #
    range = self.max_ - self.min_
    #
    my = int(self.ch /2)
    #
    #mx = int(  ( (self.value-self.min_) / range) * self.bw )
    #
    #print("in hbardiamond01 createMarker value = ",self.value," min = ", self.min_, " range  ", range, " self.bw ", self.bw, "mx = ",mx)
    originPxls = 0  # 0 from bar top is same pxls as self.max
    originPxls = originPxls #+ self.pxPadVert # accomopdate canvas to bar margin
    mx= originPxls
    #
    hh = int(self.ch/4)
    hw = int(hh/3) # for horz make sure marker is taller than wide
    #
    self.marker = self.create_polygon(
        mx,  my+hh,
        mx+hw,  my,
        mx,  my-hh,
        mx-hw,  my,
        mx,  my+hh,
        width=1        
    )
    #22oct iwhy is the shape filled? its dark
    # well red works, so the dark color is some default of i set it  dunno how where
    # buty i see the thumb is BEHIND the raiul!
    self.itemconfig(self.marker,fill="red")
