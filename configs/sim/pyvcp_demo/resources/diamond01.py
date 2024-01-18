#!/usr/bin/python3
# tkinter file to darw a 'thumb' for a pyvc_wodget 'bar'
# this file would be imported as function 'createMarker'
# but it exists as a child of the widget instance that imported it
#  so would be   used as   somewidget.createMarker()
#  typically coded as   self.createMarker()
from tkinter import *
def createMarker(self):
    range = self.max_ - self.min_
    mx = int(self.cw/2)
    originPxls = 0
    originPxls = originPxls
    my= ((self.max_-self.value) / range) * self.bh
    hw = int(self.cw/2)
    hh=int(hw/2)
    self.marker = self.create_polygon(
        mx,  my+hh,
        mx+mx,  my,
        mx,  my-hh,
        mx-mx,  my,
        mx,  my+hh,
        width=1
    )
    self.itemconfig(self.marker,fill="red")
