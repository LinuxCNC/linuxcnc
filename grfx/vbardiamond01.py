#!/usr/bin/python3
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
