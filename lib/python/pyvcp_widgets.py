#    This is a component of AXIS, a front-end for emc
#    Copyright 2007 Anders Wallin <anders.wallin@helsinki.fi>
#
#   TJP 12 04 2007
#   Rugludallur saw that spinbuttons had no initial value until after thumbs inc'd or de'c
#   TJP saw that if xml prescribed <value>1234</value> the spinbutton locked up after the inc/dec
#   it seems a new term in the __init__ may fix this
#   end TJP 12 04 2007
#
#   Added initval to checkbutton/scale for initial values,  Dallur 15 April 2007 (jarl stefansson) (jarl stefansson)
#
#   Multiple additions and amendments as per notations - ArcEye 2013
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

""" A widget library for pyVCP 
    
    The layout and composition of a Python Virtual Control Panel is specified
    with an XML file. The file must begin with <pyvcp>, and end with </pyvcp>

    In the documentation for each widget, optional tags are shown bracketed:
    [ <option>Something</option> ]
    such a tag is not required for pyVCP to work, but may add functionality or
    modify the behaviour of a widget.

    Example XML file:
    <pyvcp>
        <led>
            <size>40</size>
            <halpin>"my-led"</halpin>
        </led>
    </pyvcp>
    This will create a VCP with a single LED widget which indicates the value 
    of HAL pin compname.my-led 
"""


from Tkinter import *
from hal import *
import math
import bwidget
import time

# -------------------------------------------


class pyvcp_dial(Canvas):
    # Dial widget by tomp
    """ A dial that outputs a HAL_FLOAT 
        reacts to both mouse-wheel and mouse dragging
        <dial>
            [ <size>376</size> ]
            [ <dialcolor>"grey"</dialcolor> ]
            [ <edgecolor>"pink"</edgecolor> ] 
            [ <dotcolor>"white"</dotcolor> ]
            [ <cpr>100</cpr> ]    number of changes per rev, is # of dial tick marks, beware hi values)            
            [ <min_>-33.123456</min_> ]
            [ <max_>3.3</max_> ]
            [ <text>"Gallons per Hour"</text> ]            (knob label)
            [ <initval>123</initval> ]           (initial value a whole number must end in '.')
            [ <resolution>.001</resolution> ]          (scale value a whole number must end in '.')
            [ <halpin>"anaout"</halpin> ]
            [ <param_pin>1</param_pin>] creates param pin if > 0, set to initval, value can then be set externally, ArcEye 2013
        </dial>
                
                key bindings:
                    <Button-4>              untested no wheel mouse
                    <Button-5>              untested no wheel mouse

                    <Button1-Motion>      used internally during drag
                    <ButtonPress>          used internally to record beginning of drag
                    <ButtonRelease>          used internally at end of drag

                    <Double-1> divides scale by 10
                    <Double-2> resets scale to original value
                    <Double-3> multiplies scale by 10
        
                    <Shift-1>   shift-click resets original analog value 

                features:
                    text autoscales

    """
    # FIXME:
    # -jogging should be enabled only when the circle has focus
    #   TJP nocando:   only widgets have events, not thier 'items', the circle is an item
    
    # -circle should maintain focus when mouse over dot
    #   TJP nocando:   ditto, the circle is an item, so focus & event are not aligned to it
    
    # -jogging by dragging with the mouse could work better
    
    # -add a scaled output, scale changes when alt/ctrl/shift is held down
    #   TJP dblLeftClick divides scale by 10 , dblRightClcik muxs by 10
    
    
    n=0
    #TJP TODO: let some artists look at it, butt ugly!
    #TJP cpr is overloaded, now it means "chgs per rev" not "counts per rev"
    #TJP the tik marks could get very fine, avoid high cpr to size ratios (easily seen)
    
   
    def __init__(self,root,pycomp,halpin=None,halparam=None,param_pin=0,size=200,cpr=40,dialcolor="", \
            edgecolor="",dotcolor="grey",min_=None,max_=None, \
            text=None,initval=0,resolution=0.1, \
            **kw):
        
        pad=size/10

        self.counts = int(round(initval/resolution))
        self.out = self.counts * resolution #  float output   out
        self.origValue=initval       # in case user wants to reset the pot/valve/thingy

        #self.text3=resolution

        Canvas.__init__(self,root,width=size,height=size)
        pad2=pad-size/15
        self.circle2=self.create_oval(pad2,pad2,size-pad2,size-pad2,width=3)# edge circle
        self.itemconfig(self.circle2,fill=edgecolor,activefill=edgecolor)

        self.circle=self.create_oval(pad,pad,size-pad,size-pad)             # dial circle
        self.itemconfig(self.circle,fill=dialcolor,activefill=dialcolor)
        
        self.itemconfig(self.circle)
        self.mid=size/2
        self.r=(size-2*pad)/2
        self.alfa=0
        self.d_alfa=2*math.pi/cpr
        self.size=size

        self.funit=resolution          
        self.origFunit=self.funit        # allow restoration
        
        self.mymin=min_            
        self.mymax=max_

        self.dot = self.create_oval(self.dot_coords())
        self.itemconfig(self.dot,fill=dotcolor,activefill="black")
        self.line = self.create_line( self.mid+(self.r*1)*math.cos(self.alfa), \
                            self.mid+(self.r*1)*math.sin(self.alfa), \
                            self.mid+(self.r*1.1)*math.cos(self.alfa), \
                            self.mid+(self.r*1.1)*math.sin(self.alfa))
        self.itemconfig(self.line,arrow="last",arrowshape=(10,10,10))
        self.itemconfig(self.line,width=10)

        #TJP items get rendered in order of creation, so the knob will be behind these texts
        #TJP the font can be described with pixel size by using negative value
        self.txtroom=size/6

        # a title, if the user has supplied one
        if text!=None:
            self.title=self.create_text([self.mid,self.mid-self.txtroom],
                        text=text,font=('Arial',-self.txtroom))
        # the output
        self.dro=self.create_text([self.mid,self.mid],
                        text=str(self.out),font=('Arial',-self.txtroom))
        # the scale
        self.delta=self.create_text([self.mid,self.mid+self.txtroom], 
                        text='x '+ str(self.funit),font=('Arial',-self.txtroom))

        
        self.bind('<Button-4>',self.wheel_up)            # untested no wheel mouse
        self.bind('<Button-5>',self.wheel_down)          # untested no wheel mouse
        
        self.bind('<Button1-Motion>',self.motion)        #during drag
        self.bind('<ButtonPress>',self.bdown)                #begin of drag
        self.bind('<ButtonRelease>',self.bup)                #end of drag 

        self.bind('<Double-1>',self.chgScaleDn)            # doubleclick scales down
        self.bind('<Double-2>',self.resetScale)         # doubleclick resets scale
        self.bind('<Double-3>',self.chgScaleUp)         # doubleclick scales up

        self.bind('<Shift-1>',self.resetValue)          # shift resets value
        
        self.draw_ticks(cpr)

        self.dragstartx=0
        self.dragstarty=0

        self.dragstart=0
        self.dotcolor=dotcolor

        # create the hal pin
        if halpin == None:
            halpin = "dial."+str(pyvcp_dial.n)+".out"
        self.halpin=halpin            

        if halparam == None:
            self.param_pin = param_pin
            if self.param_pin == 1:
                halparam = "dial." + str(pyvcp_dial.n) + ".param_pin"
                self.halparam=halparam        
                pycomp.newpin(halparam, HAL_FLOAT, HAL_IN)

        pyvcp_dial.n += 1
        self.pycomp=pycomp
        pycomp.newpin(halpin, HAL_FLOAT, HAL_OUT)

        pycomp[self.halparam] = self.origValue
        self.oldValue = self.origValue
        self.value = self.origValue

    def chgScaleDn(self,event):
        # reduces the scale by 10x
        self.funit=self.funit/10.0
        self.counts *= 10
        self.update_scale()
        self.update_dro()
        self.update_dot()
    
    def chgScaleUp(self,event):
        # increases the scale by 10x
        self.funit=self.funit*10.0
        self.counts = (self.counts + 5) / 10
        self.out = self.counts * self.funit
        self.update_scale()
        self.update_dro()
        self.update_dot()
    
    def resetScale(self,event):
        # reset scale to original value
        self.funit=self.origFunit
        self.counts = int(round(self.out / self.funit))
        self.out = self.counts * self.funit
        self.update_scale()
    
    def resetValue(self,event):
        # reset output to orifinal value
        self.counts = int(round(self.origValue / self.funit))
        self.out= self.counts * self.funit
        self.update_dot()
        self.update_dro()

    def dot_coords(self):
        # calculate the coordinates for the dot
        DOTR=0.04*self.size
        DOTPOS=0.85
        midx = self.mid+DOTPOS*self.r*math.cos(self.alfa)
        midy = self.mid+DOTPOS*self.r*math.sin(self.alfa)
        return midx-DOTR, midy-DOTR,midx+DOTR,midy+DOTR

    def bdown(self,event):
        self.dragstartx=event.x
        self.dragstarty=event.y
        self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
        self.itemconfig(self.dot,fill="black",activefill="black")

    def bup(self,event):
        self.itemconfig(self.dot,fill=self.dotcolor)

    def motion(self,event):
        dragstop = math.atan2((event.y-self.mid),(event.x-self.mid))
        delta = dragstop - self.dragstart
        if delta>=self.d_alfa:
            self.up()
            self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
        elif delta<=-self.d_alfa:
            self.down()
            self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
        self.itemconfig(self.dot,fill="black",activefill="black")

    def wheel_up(self,event):
        self.up()

    def wheel_down(self,event):
        self.down()

    def down(self):
        self.alfa-=self.d_alfa
        self.counts -= 1
        self.out = self.counts * self.funit
        #TJP clip down side
        if self.mymin != None:
            if self.out<self.mymin:
                self.out=self.mymin
                self.counts = self.mymin * self.funit
        self.update_dot()
        self.update_dro()

    def up(self):
        self.alfa+=self.d_alfa
        self.counts += 1
        self.out = self.counts * self.funit
        #TJP clip up side
        if self.mymax != None:
            if self.out>self.mymax:
                self.out=self.mymax
                self.counts = self.mymax * self.funit
        self.update_dot()
        self.update_dro()

    def update_dot(self):
        self.coords(self.dot, self.dot_coords() )
        self.coords(self.line, self.mid+(self.r*1)*math.cos(self.alfa),self.mid+(self.r*1)*math.sin(self.alfa), \
                            self.mid+(self.r*1.1)*math.cos(self.alfa), \
                            self.mid+(self.r*1.1)*math.sin(self.alfa))  

    def update_dro(self):
        valtext = str(self.out)
        self.itemconfig(self.dro,text=valtext)

    def update_scale(self):
        valtext = str(self.funit)
        valtext = 'x ' + valtext
        self.itemconfig(self.delta,text=valtext)

    def draw_ticks(self,cpr):
        for n in range(0,cpr,2):
           for i in range(0,2):
            startx=self.mid+self.r*math.cos((n+i)*self.d_alfa)
            starty=self.mid+self.r*math.sin((n+i)*self.d_alfa)
            if i == 0:
               length = 1.15
               width = 2
            else:
               length = 1.1
               width = 1
            stopx=self.mid+length*self.r*math.cos((n+i)*self.d_alfa)
            stopy=self.mid+length*self.r*math.sin((n+i)*self.d_alfa)
            self.create_line(startx,starty,stopx,stopy,width=width)

    def update(self,pycomp):
        self.pycomp[self.halpin] = self.out

        self.value = pycomp[self.halparam]
        if self.value != self.oldValue :
            self.counts = int(round(self.value / self.funit))
            self.out= self.counts * self.funit
            self.update_dot()
            self.update_dro()
            self.oldValue = self.value

# -------------------------------------------



class pyvcp_meter(Canvas):
    """ Meter - shows the value of a FLOAT with an analog meter
        <meter>
            [ <size>300</size> ]
            [ <halpin>"mymeter"</halpin> ]
            [ <text>"My Voltage"</text> ]
            [ <subtext>"Volts"</subtext>
            [ <min_>-22</min_> ]
            [ <max_>123</max_> ]
            [ <majorscale>10</majorscale> ]
            [ <minorscale>5</minorscale> ]
            [ <region1>(70,80,"green")</region1> ]
            [ <region2>(80,100,"orange")</region2> ]
            [ <region3>(100,123,"red")</region3> ]
        </meter>
    """
    # FIXME: logarithmic scale option
    n=0
    def __init__(self,root,pycomp,halpin=None, size=200,text=None,subtext=None,min_=0,max_=100,majorscale=None, minorscale=None,region1=None,region2=None,region3=None,**kw):
        self.size = size
        self.pad=10
        Canvas.__init__(self,root,width=size,height=size)
        self.halpin=halpin
        self.min_=min_
        self.max_=max_
        range_=2.5
        self.min_alfa=-math.pi/2-range_
        self.max_alfa=-math.pi/2+range_
        self.circle=self.create_oval(self.pad,self.pad,size-self.pad,size-self.pad, width=2)
        self.itemconfig(self.circle,fill="white")
        self.mid=size/2
        self.r=(size-2*self.pad)/2
        self.alfa=0
        if minorscale==None: 
            self.minorscale=0
        else:
            self.minorscale=minorscale
        if majorscale==None: 
            self.majorscale=float((self.max_-self.min_)/10)
        else: 
            self.majorscale=majorscale
        if text!=None: t=self.create_text([self.mid,self.mid-size/12],font="Arial %d bold" % (size/10),text=text)
        if subtext!=None: t=self.create_text([self.mid,self.mid+size/12],font="Arial %d" % (size/30+5),text=subtext)
        if region1!=None: self.draw_region(region1)
        if region2!=None: self.draw_region(region2)
        if region3!=None: self.draw_region(region3)
        self.draw_ticks()

        self.line = self.create_line([self.mid,self.mid, self.mid+self.r*math.cos(self.alfa), self.mid+self.r*math.sin(self.alfa)],fill="red", arrow="last", arrowshape=(0.9*self.r,self.r,self.r/20))
        self.itemconfig(self.line,width=3)

        # create the hal pin
        if halpin == None:
            self.halpin = "meter."+str(pyvcp_meter.n)+".value"
            pyvcp_meter.n += 1
        pycomp.newpin(self.halpin, HAL_FLOAT, HAL_IN)
        self.value = pycomp[self.halpin]
    
    def rad2deg(self, rad): return rad*180/math.pi

    def value2angle(self, value):
            #returns angle for a given value
            scale = (self.max_-self.min_)/(self.max_alfa-self.min_alfa)
            alfa = self.min_alfa + (value-self.min_)/scale
            if alfa > self.max_alfa:
                alfa = self.max_alfa
            elif alfa < self.min_alfa:
                alfa = self.min_alfa            
            return alfa
    
    def p2c(self, radius, angle): 
        #returns the cathesian coordinates (x,y) for given polar coordinates 
        #radius in percent of self.r; angle in radians
        return self.mid+radius*self.r*math.cos(angle), self.mid+radius*self.r*math.sin(angle)

    def update(self,pycomp):
        self.value = pycomp[self.halpin]
        self.alfa = self.value2angle(self.value)
        x,y = self.p2c(0.8, self.alfa)
        self.coords(self.line,self.mid,self.mid,x,y)

    def draw_region(self, (start, end, color)):
            #Draws a colored region on the canvas between start and end
            start = self.value2angle(start)
            start = -self.rad2deg(start)
            end = self.value2angle(end)
            end = -self.rad2deg(end)
            extent = end-start
            halfwidth = math.floor(0.1*self.r/2)+1
            xy = self.pad+halfwidth, self.pad+halfwidth, self.size-self.pad-halfwidth, self.size-self.pad-halfwidth
            self.create_arc(xy, start=start, extent=extent, outline=color, width=(halfwidth-1)*2, style="arc")

    def draw_ticks(self):
        value = self.min_
        while value <= self.max_:
            alfa = self.value2angle(value)
            xy1 = self.p2c(1,alfa)
            xy2 = self.p2c(0.85,alfa)
            xytext = self.p2c(0.75,alfa)
            self.create_text(xytext,font="Arial %d" % (self.size/30+5), text="%g" % value)
            self.create_line(xy1, xy2, width=2)
            value = value + self.majorscale
        #minor ticks
        value = self.min_
        if self.minorscale > 0:
            while value <= self.max_:
                if (value % self.majorscale) != 0:
                    alfa = self.value2angle(value)
                    xy1 = self.p2c(1,alfa)
                    xy2 = self.p2c(0.9,alfa)
                    self.create_line(xy1, xy2)
                value = value + self.minorscale

             
# -------------------------------------------

class pyvcp_jogwheel(Canvas):
    """" A jogwheel that outputs a HAL_FLOAT count
        reacts to both mouse-wheel and mouse dragging
        <jogwheel>
            [ <cpr>33</cpr> ]                       (counts per revolution)
            [ <halpin>"myjogwheel"</halpin> ]
            [ <size>300</size> ]
         </jogwheel>
    """
    # FIXME:
    # -jogging should be enabled only when the circle has focus
    # -circle should maintain focus when mouse over dot
    # -jogging by dragging with the mouse could work better
    # -add a scaled output, scale changes when alt/ctrl/shift is held down
    n=0
    def __init__(self,root,pycomp,halpin=None,size=200,cpr=40,**kw):
        pad=size/10
        self.count=0
        Canvas.__init__(self,root,width=size,height=size)
        pad2=pad-size/15
        self.circle2=self.create_oval(pad2,pad2,size-pad2,size-pad2,width=3)# edge circle
        self.circle=self.create_oval(pad,pad,size-pad,size-pad)
        self.itemconfig(self.circle,fill="lightgrey",activefill="lightgrey")
        self.mid=size/2
        self.r=(size-2*pad)/2
        self.alfa=0
        self.d_alfa=2*math.pi/cpr
        self.size=size
        
        
        self.dot = self.create_oval(self.dot_coords())
        self.itemconfig(self.dot,fill="black")
        self.line = self.create_line( self.mid+(self.r*1)*math.cos(self.alfa), \
                            self.mid+(self.r*1)*math.sin(self.alfa), \
                            self.mid+(self.r*1.1)*math.cos(self.alfa), \
                            self.mid+(self.r*1.1)*math.sin(self.alfa))
        self.itemconfig(self.line,arrow="last",arrowshape=(10,10,10))
        self.itemconfig(self.line,width=8)

        self.bind('<Button-4>',self.wheel_up)
        self.bind('<Button-5>',self.wheel_down)
        self.bind('<Button1-Motion>',self.motion)
        self.bind('<ButtonPress>',self.bdown)
        self.draw_ticks(cpr)
        self.dragstartx=0
        self.dragstarty=0
        self.dragstart=0

        # create the hal pin
        if halpin == None:
            halpin = "jogwheel."+str(pyvcp_jogwheel.n)+".count"
            pyvcp_jogwheel.n += 1
        pycomp.newpin(halpin, HAL_FLOAT, HAL_OUT)
        self.halpin=halpin
        pycomp[self.halpin] = self.count
        self.pycomp=pycomp
                 
    def dot_coords(self):
        DOTR=0.06*self.size
        DOTPOS=0.85
        midx = self.mid+DOTPOS*self.r*math.cos(self.alfa)
        midy = self.mid+DOTPOS*self.r*math.sin(self.alfa)
        return midx-DOTR, midy-DOTR,midx+DOTR,midy+DOTR
    
    def bdown(self,event):
        self.dragstartx=event.x
        self.dragstarty=event.y
        self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))

    def motion(self,event):
        dragstop = math.atan2((event.y-self.mid),(event.x-self.mid))
        delta = dragstop - self.dragstart
        if delta>=self.d_alfa:
            self.up()
            self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
        elif delta<=-self.d_alfa:
            self.down()
            self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
    
    def wheel_up(self,event):
        self.up()
        
    def wheel_down(self,event):
        self.down()

    def down(self):
        self.alfa-=self.d_alfa
        self.count-=1
        self.pycomp[self.halpin] = self.count
        self.update_dot()       
    
    def up(self):
        self.alfa+=self.d_alfa
        self.count+=1
        self.pycomp[self.halpin] = self.count
        self.update_dot()  

    def update_dot(self):
        self.coords(self.dot, self.dot_coords() )  
        self.coords(self.line, self.mid+(self.r*1)*math.cos(self.alfa),self.mid+(self.r*1)*math.sin(self.alfa), \
                            self.mid+(self.r*1.1)*math.cos(self.alfa), \
                            self.mid+(self.r*1.1)*math.sin(self.alfa))         

    def draw_ticks(self,cpr):
        for n in range(0,cpr):
            startx=self.mid+self.r*math.cos(n*self.d_alfa)
            starty=self.mid+self.r*math.sin(n*self.d_alfa)
            stopx=self.mid+1.15*self.r*math.cos(n*self.d_alfa)
            stopy=self.mid+1.15*self.r*math.sin(n*self.d_alfa)
            self.create_line([startx,starty,stopx,stopy])

    def update(self,pycomp):
        # this is stupid, but required for updating pin
        # when first connected to a signal
        self.pycomp[self.halpin] = self.count
        
# -------------------------------------------
## ArcEye - added - no example given and the one in docs misses out initval  ##

"""" A radiobutton will set one of the halpins true. The other pins are set false.
    <radiobutton>
        <choices>["one","two","three"]</choices>    labels next to each button
        <halpin>"radio"</halpin>                    pin giving index of active button
        <initval>0</initval>                        index of button pin to set true at start
        <orient>HORIZONTAL</orient>                 add horizontal tag default is vertical
    </radiobutton>
    """
################################################################################

class pyvcp_radiobutton(Frame):
    n=0
    def __init__(self,master,pycomp,halpin=None,initval=0,orient=None,choices=[],**kw):
        f=Frame.__init__(self,master,bd=2,relief=GROOVE)
        self.v = IntVar()
        self.v.set(1)
        self.choices=choices
        self.side = 'top'
        if orient != None:
            self.side = 'left'
        if halpin == None:
            halpin = "radiobutton."+str(pyvcp_radiobutton.n)
            pyvcp_radiobutton.n += 1
        
        self.halpins=[]
        n=0
        for c in choices:
            b=Radiobutton(self,f, text=str(c),variable=self.v, value=pow(2,n))
            b.pack(side=self.side)
            if n==initval: 
                b.select()
            c_halpin=halpin+"."+str(c)
            pycomp.newpin(c_halpin, HAL_BIT, HAL_OUT)
            self.halpins.append(c_halpin)
            n+=1

        self.selected = initval
        pycomp[self.halpins[initval]]=1 

## ArcEye - FIXED - only update the pins if changed  ##
    def update(self,pycomp):
        index=int(math.log(self.v.get(),2))
        if index != self.selected: 
            for pin in self.halpins:
                pycomp[pin]=0;
            pycomp[self.halpins[index]]=1;
            self.selected = index


    # FIXME
    # this would be a much better way of updating the
    # pins, but at the moment I can't get it to work
    # this is never called even if I set command=self.update()
    # in the call to Radiobutton above
    def changed(self):
        index=math.log(self.v.get(),2)
        index=int(index)
        print "active:",self.halpins[index]



# -------------------------------------------

class pyvcp_label(Label):
    """ Static text label 
        <label>
            <text>"My Label:"</text>
            <halpin>"name"</halpin>
            <disable_pin>True</disable_pin>
        </label>
    """
    n=0
    def __init__(self,master,pycomp,halpin=None,disable_pin=False,**kw):
        Label.__init__(self,master,**kw)
        self.disable_pin=disable_pin
        if disable_pin:
            if halpin == None:
                halpin = "label."+str(pyvcp_label.n) 
                pyvcp_label.n += 1
            halpin_disable = halpin+".disable"
            self.halpin_disable = halpin_disable
            pycomp.newpin(halpin_disable, HAL_BIT, HAL_IN)   
        
    def update(self,pycomp):
        if self.disable_pin: 
            is_disabled = pycomp[self.halpin_disable]     
            if is_disabled == 1: Label.config(self,state=DISABLED)
            else: Label.config(self,state=NORMAL)
        else:pass
       


# -------------------------------------------

### ArcEye 01052013 - added new widget #############################################

class pyvcp_multilabel(Label):
    """ Selectable text label, can display up to 6 label legends
        when associated bit pin is activated
        <multilabel>
            <legends>["Label1" "Label2" "Label3" "Label4" "Label5" "Label6"]</legends>
            <font>("Helvetica", 20)</font>
            <disable_pin>True</disable_pin>
            <initval>0</initval>    Which legend to display by default (make sure it matches initval for linked widget if any) 
            <halpin>None</halpin>   Optional alternative name base for pins
        </multilabel>
    """
    
    n=0
    def __init__(self,master,pycomp,halpin=None,disable_pin=False,legends=[],initval=0,**kw):
        Label.__init__(self,master,**kw)
        self.disable_pin=disable_pin
        
        if halpin == None:
            halpin = "multilabel."+str(pyvcp_multilabel.n)
            pyvcp_multilabel.n += 1
        
        self.halpins=[]
        n=0
        for c in legends:
            c_halpin=halpin+".legend"+str(n)
            pycomp.newpin(c_halpin, HAL_BIT, HAL_IN)
            self.halpins.append(c_halpin)
            n+=1
            #limit to 6 legends
            if n >= 6:
                break
        self.legends = legends
        self.num_pins = n
        
        if disable_pin:
            halpin_disable = halpin+".disable"
            self.halpin_disable = halpin_disable
            pycomp.newpin(halpin_disable, HAL_BIT, HAL_IN)

        # test for out of range initval number
        if initval >= 0 and initval <= self.num_pins :
            val = initval
        else :
            val = 0
            
        Label.config(self,text= legends[val])
        pycomp[self.halpins[val]] = 1
        self.pin_index = val

    def update(self,pycomp):
        if self.disable_pin: 
            is_disabled = pycomp[self.halpin_disable]     
            if is_disabled == 1: 
                Label.config(self,state=DISABLED)
            else: 
                Label.config(self,state=NORMAL)
                
        # no telling how people will use this, so just break at first
        # set pin that is not the existing set one
        # if several pins are set one after another, the legend for the
        # last one set will end up being displayed
        index = -1
        for x in xrange(0, self.num_pins):
            state = pycomp[self.halpins[x]]
            if state == 1 :
                index = x
                if index != self.pin_index :
                    break
                
        if index > -1 and index != self.pin_index:
            for x in xrange(0, self.num_pins):
                pycomp[self.halpins[x]] = 0

            pycomp[self.halpins[index]] = 1
            Label.config(self,text= self.legends[index])
            self.pin_index = index



class pyvcp_vbox(Frame):
    """ Box in which widgets are packed vertically
        <vbox>
            <relief>GROOVE</relief>         (FLAT, SUNKEN, RAISED, GROOVE, RIDGE)
            <bd>3</bd>                      (border width)
            place widgets here
        </vbox>
    """
    def __init__(self,master,pycomp,bd=0,relief=FLAT):
        Frame.__init__(self,master,bd=bd,relief=relief)
        self.fill = 'x'
        self.side = 'top'
        self.anchor = 'center'
        self.expand = 'yes'

    def update(self,pycomp): 
        pass

    def add(self, container, widget):
        if isinstance(widget, pyvcp_boxexpand):
            self.expand = widget.expand
            return
        if isinstance(widget, pyvcp_boxfill):
            self.fill = widget.fill
            return
        if isinstance(widget, pyvcp_boxanchor):
            self.anchor = widget.anchor
            return
        widget.pack(side=self.side, anchor=self.anchor, fill=self.fill, expand=self.expand)

class pyvcp_boxfill:
    def __init__(self, master, pycomp, fill):
        self.fill = fill
    def update(self, pycomp): pass

class pyvcp_boxanchor:
    def __init__(self, master, pycomp, anchor):
        self.anchor = anchor
    def update(self, pycomp): pass

class pyvcp_boxexpand:
    def __init__(self, master, pycomp, expand):
        self.expand = expand
    def update(self, pycomp): pass

# -------------------------------------------

class pyvcp_hbox(Frame):
    """ Box in which widgets are packed horizontally
        <vbox>
            <relief>GROOVE</relief>         (FLAT, SUNKEN, RAISED, GROOVE, RIDGE)
            <bd>3</bd>                      (border width)
            place widgets here
        </vbox>        
    """
    def __init__(self,master,pycomp,bd=0,relief=FLAT):
        Frame.__init__(self,master,bd=bd,relief=relief)
        self.fill = 'y'
        self.side = 'left'
        self.anchor = 'center'
        self.expand = 'yes'

    def update(self,pycomp): 
        pass

    def add(self, container, widget):
        if isinstance(widget, pyvcp_boxexpand):
            self.expand = widget.expand
            return
        if isinstance(widget, pyvcp_boxfill):
            self.fill = widget.fill
            return
        if isinstance(widget, pyvcp_boxanchor):
            self.anchor = widget.anchor
            return
        widget.pack(side=self.side, anchor=self.anchor, fill=self.fill)

class pyvcp_labelframe(LabelFrame):
    """
     frame with a title
    """
    def __init__(self,master,pycomp,**kw):
        LabelFrame.__init__(self,master,**kw)
        self.pack(expand=1,fill=BOTH)
    def update(self,pycomp):
        pass
    def add(self, container, widget):
        widget.pack(side="top", fill="both", expand="yes")

class pyvcp_tabs(bwidget.NoteBook):
    def __init__(self, master, pycomp, cnf={}, **kw):
        self.names = kw.pop("names", [])
        self.idx = 0
        self._require(master)
        Widget.__init__(self, master, "NoteBook", cnf, kw)

    def update(self, pycomp): pass

    def add(self, container, child):
        child.pack(side="top", fill="both", anchor="ne")
        if self.idx == 1:
            self.raise_page(self.names[0])

    def getcontainer(self):
        if len(self.names) < self.idx:
            self.names.append("Tab-%d" % self.idx)
        name = self.names[self.idx]
        self.idx += 1
        return self.insert("end", name, text=name)

# -------------------------------------------

class pyvcp_spinbox(Spinbox):
    """ (control) controls a float, also shown as text 
        reacts to the mouse wheel 
        <spinbox>
            [ <halpin>"my-spinbox"</halpin> ]
            [ <min_>55</min_> ]   sets the minimum value to 55
            [ <max_>123</max_> ]  sets the maximum value to 123
            [ <initval>100</initval> ]  sets initial value to 100  TJP 12 04 2007
            [ <param_pin>1</param_pin>] creates param pin if > 0, set to initval, value can then be set externally, ArcEye 2013            
        </spinbox>
    """
    # FIXME: scale resolution when shift/ctrl/alt is held down?
 
    n=0
    def __init__(self,master,pycomp,halpin=None, halparam=None,param_pin=0,
                    min_=0,max_=100,initval=0,resolution=1,format="2.1f",**kw):
        self.v = DoubleVar()
        if 'increment' not in kw: kw['increment'] = resolution
        if 'from' not in kw: kw['from'] = min_
        if 'to' not in kw: kw['to'] = max_
        if 'format' not in kw: kw['format'] = "%" + format
        kw['command'] = self.command
        Spinbox.__init__(self,master,textvariable=self.v,**kw)
        
        if halpin == None:
            halpin = "spinbox."+str(pyvcp_spinbox.n)
            
        self.halpin=halpin

        if halparam == None:
            self.param_pin = param_pin
            if self.param_pin == 1:
                halparam = "spinbox." + str(pyvcp_spinbox.n) + ".param_pin"
                self.halparam=halparam
                pycomp.newpin(halparam, HAL_FLOAT, HAL_IN)

        pyvcp_spinbox.n += 1
        
        if initval < min_:
            self.value=min_
        elif initval > max_:
            self.value=max_
        else:
            self.value=initval
        self.oldvalue=min_

        if self.param_pin == 1:
            self.init=self.value
            self.oldinit=self.init
            pycomp[self.halparam] = self.init
            
        self.format = "%(b)"+format
        self.max_=max_
        self.min_=min_
        self.resolution=resolution
        self.v.set( str( self.format  % {'b':self.value} ) )
        pycomp.newpin(halpin, HAL_FLOAT, HAL_OUT)
        
        self.bind('<Button-4>',self.wheel_up)
        self.bind('<Button-5>',self.wheel_down)
        self.bind('<Return>',self.return_pressed)

    def return_pressed(self, event):
        self.value = self.v.get()
        if self.value < self.min_:
            self.value = self.min_
        if self.value > self.max_:
            self.value = self.max_


    def command(self):
        self.value = self.v.get()

    def update(self,pycomp):
        pycomp[self.halpin] = self.value
        if self.value != self.oldvalue:
            self.v.set( str( self.format  % {'b':self.value} ) )
            self.oldvalue=self.value

        if self.param_pin == 1:            
            self.init = pycomp[self.halparam]
            if self.init != self.oldinit:
                self.v.set( str( self.format  % {'b':self.init} ) )
                self.oldinit=self.init    
                self.value=self.init
          
    def wheel_up(self,event):
        self.value += self.resolution
        if self.value > self.max_:
            self.value = self.max_
          
     
    def wheel_down(self,event):
        self.value -= self.resolution
        if self.value < self.min_:
            self.value = self.min_
          


# -------------------------------------------

class pyvcp_number(Label):
    """ (indicator) shows a float as text """
    n=0
    def __init__(self,master,pycomp,halpin=None,format="2.1f",**kw):
        self.v = StringVar()
        self.format=format
        Label.__init__(self,master,textvariable=self.v,**kw)
        if halpin == None:
            halpin = "number."+str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin=halpin
        self.value=0.0
        dummy = "%(b)"+self.format
        self.v.set( str( dummy  % {'b':self.value} ) )
        pycomp.newpin(halpin, HAL_FLOAT, HAL_IN)

    def update(self,pycomp):    
        newvalue = pycomp[self.halpin]
        if newvalue != self.value:
            self.value=newvalue
            dummy = "%(b)"+self.format
            self.v.set( str( dummy  % {'b':newvalue} ) )


class pyvcp_u32(Label):
    """ (indicator) shows a u32 as text """
    n=0
    def __init__(self,master,pycomp,halpin=None,format="d",**kw):
        self.v = StringVar()
        self.format=format
        Label.__init__(self,master,textvariable=self.v,**kw)
        if halpin == None:
            halpin = "number."+str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin=halpin
        self.value=0.0
        dummy = "%(b)"+self.format
        self.v.set( str( dummy  % {'b':self.value} ) )
        pycomp.newpin(halpin, HAL_U32, HAL_IN)

    def update(self,pycomp):    
        newvalue = pycomp[self.halpin]
        if newvalue != self.value:
            self.value=newvalue
            dummy = "%(b)"+self.format
            self.v.set( str( dummy  % {'b':newvalue} ) )

 
class pyvcp_s32(Label):
    """ (indicator) shows a s32 as text """
    n=0
    def __init__(self,master,pycomp,halpin=None,format="d",**kw):
        self.v = StringVar()
        self.format=format
        Label.__init__(self,master,textvariable=self.v,**kw)
        if halpin == None:
            halpin = "number."+str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin=halpin
        self.value=0.0
        dummy = "%(b)"+self.format
        self.v.set( str( dummy  % {'b':self.value} ) )
        pycomp.newpin(halpin, HAL_S32, HAL_IN)

    def update(self,pycomp):    
        newvalue = pycomp[self.halpin]
        if newvalue != self.value:
            self.value=newvalue
            dummy = "%(b)"+self.format
            self.v.set( str( dummy  % {'b':newvalue} ) )

class pyvcp_timer(Label):
    """ (indicator) shows elapsed time as HH:MM:SS
    two pins - run and reset
    time advances whenever run is true
    time holds whenever run is false
    time resets to zero on a rising edge of reset
    """
    n=0
    def __init__(self,master,pycomp,halpin=None,**kw):
        self.v = StringVar()
        Label.__init__(self,master,textvariable=self.v,**kw)
        if halpin == None:
            halpin = "timer."+str(pyvcp_timer.n)
            pyvcp_timer.n += 1
        self.halpins=[]
        c_halpin=halpin+".reset"
        pycomp.newpin(c_halpin, HAL_BIT, HAL_IN)
        self.halpins.append(c_halpin)
        c_halpin=halpin+".run"
        pycomp.newpin(c_halpin, HAL_BIT, HAL_IN)
        self.halpins.append(c_halpin)

        self.resetvalue=0
        self.runvalue=0
        # starttime is the time of the last rising edge of 'run'
        self.starttime=0
        # basetime is the sum of all prior 'run=1' periods
        self.basetime=0
        self.currtime=0
        self.v.set( "00:00:00")


    def update(self,pycomp):    
        resetvalue = pycomp[self.halpins[0]]
        runvalue = pycomp[self.halpins[1]]
        if resetvalue != self.resetvalue:
            self.resetvalue=resetvalue
            if resetvalue == 1:
                self.basetime=0
                self.starttime=time.time()
        if runvalue != self.runvalue:
            self.runvalue=runvalue
            if runvalue == 1:
                # rising edge
                self.starttime = time.time()
            else:
                # falling edge
                self.basetime += time.time() - self.starttime
        if runvalue == 1:
            total=self.basetime + time.time() - self.starttime
        else:
            total=self.basetime
        hr = int(total / 3600)
        remainder = total - hr*3600
        mn = int(remainder / 60)
        sec = int(remainder - mn*60)
        self.v.set( str( "%02d:%02d:%02d" % (hr,mn,sec) ) )


# -------------------------------------------

class pyvcp_bar(Canvas):

## reworked by ArcEye 10022014
## allow value ranges in different colours

    """ (indicator) a bar-indicator for a float
    <bar>
        <halpin>"my-bar"</halpin>
        <min_>0</min_>
        <max_>150</max_>
        <bgcolor>"grey"</bgcolor>
        <range1>(0,100,"green")</range1>
        <range2>(101,129,"orange")</range2>
        <range3>(130,150,"red")</range3>
        <fillcolor>"green"</fillcolor>
        <canvas_width>200</canvas_width>
        <canvas_height>50</canvas_height>
        <bar_height>30</bar_height>
        <bar_width>150</bar_width>
        <format>"3.1f"</format>
    </bar>
    """
    n=0

    def __init__(self,master,pycomp,fillcolor="green",bgcolor="grey",
               halpin=None,min_=0.0,max_=100.0,range1=None,range2=None,
               range3=None,format='3.1f', canvas_width=200,
               canvas_height=50, bar_height=30, bar_width=150,**kw):

        self.cw=canvas_width
        self.ch=canvas_height
        self.bh=bar_height
        self.bw=bar_width
        #self.cw=200    # canvas width
        #self.ch=50     # canvas height
        #self.bh=30     # bar height
        #self.bw=150    # bar width
        self.pad=((self.cw-self.bw)/2)

        Canvas.__init__(self,master,width=self.cw,height=self.ch)

        if halpin == None:
            halpin = "bar."+str(pyvcp_bar.n)
            pyvcp_bar.n += 1
        self.halpin=halpin
        self.endval=max_
        self.startval=min_
        self.format = "%" + format

        pycomp.newpin(halpin, HAL_FLOAT, HAL_IN)
        
        self.value=0.0 # some dummy value to start with  
             
        pycomp[self.halpin] = self.value
        
        # the border
        border=self.create_rectangle(self.pad,1,self.pad+self.bw,self.bh)
        self.itemconfig(border,fill=bgcolor)
        
        # the bar
        tmp=self.bar_coords()
        start=tmp[0]
        end=tmp[1]
        self.bar=self.create_rectangle(start,2,end,self.bh-1)
	    # default fill unless overriden
        self.itemconfig(self.bar,fill=fillcolor)

        # start text
        start_text=self.create_text(self.pad,self.bh+10,text=str(self.startval) )
        #end text
        end_text=self.create_text(self.pad+self.bw,self.bh+10,text=str(self.endval) )
        # value text
        self.val_text=self.create_text(self.pad+self.bw/2,
                                   self.bh/2,text=str(self.value) )

        if range1!=None and range2!=None and range3!=None:
            self.range1 = range1
            self.range2 = range2
            self.range3 = range3
            self.ranges = True
        else:
            self.ranges = False
        
    def set_fill(self, (start1, end1, color1),(start2, end2, color2), (start3, end3, color3)):
        if self.value:
    	    if (self.value > start1) and (self.value <= end1):
    		self.itemconfig(self.bar,fill=color1)	
    	    else:
    		if (self.value > start2) and (self.value <= end2):
    		    self.itemconfig(self.bar,fill=color2)	
		else:
		    if (self.value > start3) and (self.value <= end3):
    			self.itemconfig(self.bar,fill=color3)	
	
	
    def bar_coords(self):
        """ calculates the coordinates in pixels for the bar """
        # the bar should start at value = zero 
        # and extend to value = self.value
        # it should not extend beyond the initial box reserved for the bar
        min_pixels=self.pad
        max_pixels=self.pad+self.bw
        bar_end = min_pixels + ((float)(max_pixels-min_pixels)/(float)(self.endval-self.startval)) * (self.value-self.startval)
        if bar_end>max_pixels:
            bar_end = max_pixels
        elif bar_end < min_pixels:
            bar_end = min_pixels
        bar_start = min_pixels + ((float)(max_pixels-min_pixels)/(float)(self.endval-self.startval)) * (0-self.startval)
        if bar_start < min_pixels:  # don't know if this is really needed
            bar_start = min_pixels

        return [bar_start, bar_end]
    
			
    def update(self,pycomp):
        # update value
        newvalue=pycomp[self.halpin]
        if newvalue != self.value:
            self.value = newvalue
            valtext = str(self.format % self.value)
            self.itemconfig(self.val_text,text=valtext)
            # set bar colour
            if self.ranges:
                self.set_fill(self.range1, self.range2, self.range3)
            # set bar size
            tmp=self.bar_coords()
            start=tmp[0]
            end=tmp[1]
            self.coords(self.bar, start, 2, 
                        end, self.bh-1)



# -------------------------------------------




class pyvcp_led(Canvas):
    """ (indicator) a LED 
        <led>
            <on_color>"colorname"</on_color>             Default color red
            <off_color>"colorname"</off_color>           Default color green
            <disable_pin>True</disable_pin>               Optional halpin sets led to disable_color
            <disable_color>"colorname"</disable_color>   Default color gray80
        </led>"""
    n=0
    def __init__(self,master,pycomp, halpin=None,disable_pin=False,     
                    off_color="red",on_color="green",disabled_color="gray80",size=20,**kw):
        Canvas.__init__(self,master,width=size,height=size,bd=0)
        self.off_color=off_color
        self.on_color=on_color
        self.disabled_color=disabled_color
        self.disable_pin = disable_pin
        self.oh=self.create_oval(1,1,size,size)
        self.state = 0
        self.itemconfig(self.oh,fill=off_color)
        if halpin == None:
            halpin = "led."+str(pyvcp_led.n) 
            pyvcp_led.n+=1
        self.halpin=halpin
        pycomp.newpin(halpin, HAL_BIT, HAL_IN)
        if disable_pin:
            halpin_disable = halpin+".disable"
            self.halpin_disable = halpin_disable
            pycomp.newpin(halpin_disable, HAL_BIT, HAL_IN)       
        

    def update(self,pycomp):
        newstate = pycomp[self.halpin]
        if newstate == 1:
            self.itemconfig(self.oh,fill=self.on_color)
            self.state=1
        else:
            self.itemconfig(self.oh,fill=self.off_color) 
            self.state=0
        if self.disable_pin:
            is_disabled = pycomp[self.halpin_disable]
            if is_disabled == 1:
                self.itemconfig(self.oh,fill=self.disabled_color)




# -------------------------------------------

class pyvcp_rectled(Canvas):

    """ (indicator) a LED 
        <rectled>
            <on_color>"colorname"</on_color>             Default color red
            <off_color>"colorname"</off_color>           Default color green
            <disable_pin>True</disable_pin>               Optional halpin sets led to disable_color
            <disable_color>"somecolor"</disable_color>   Default color light gray
        </rectled>"""
    
    n=0
    def __init__(self,master,pycomp, halpin=None,disable_pin=False,     
                    off_color="red",on_color="green",disabled_color="gray80",height=10,width=30,**kw):
        Canvas.__init__(self,master,width=width,height=height,bd=2)
        self.off_color=off_color
        self.on_color=on_color
        self.disabled_color=disabled_color
        self.disable_pin = disable_pin
        self.oh=self.create_rectangle(1,1,width,height)
        self.state=0
        self.itemconfig(self.oh,fill=off_color)
        if halpin == None:
            halpin = "led."+str(pyvcp_led.n)  
            pyvcp_led.n+=1     
        self.halpin=halpin
        pycomp.newpin(halpin, HAL_BIT, HAL_IN)
        if disable_pin:
            halpin_disable = halpin+".disable"
            self.halpin_disable = halpin_disable
            pycomp.newpin(halpin_disable, HAL_BIT, HAL_IN)   
        

    def update(self,pycomp):
        newstate = pycomp[self.halpin]
        if newstate == 1:
            self.itemconfig(self.oh,fill=self.on_color)
            self.state=1
        else:
            self.itemconfig(self.oh,fill=self.off_color) 
            self.state=0
        if self.disable_pin:
            is_disabled = pycomp[self.halpin_disable]
            if is_disabled == 1:
                self.itemconfig(self.oh,fill=self.disabled_color)




# -------------------------------------------

## ArcEye - the initval field is missing from the docs, so few people aware it can be preselected
## 12022014 added changepin which allows toggling of value from HAL without GUI action

class pyvcp_checkbutton(Checkbutton):

    """ (control) a check button 
        halpin is 1 when button checked, 0 otherwise 
        <checkbutton>
            [ <halpin>"my-checkbutton"</halpin> ]
            [ <text>"Name of Button"</text>]  text set in widget
            [ <initval>1</initval> ]  sets initial value to 1, all values >=0.5 are assumed to be 1
        </checkbutton>
    """
    n=0
    def __init__(self,master,pycomp,halpin=None,initval=0,**kw):
        self.v = BooleanVar(master)
        Checkbutton.__init__(self,master,variable=self.v,onvalue=1, offvalue=0,**kw)
        if halpin == None:
            halpin = "checkbutton."+str(pyvcp_checkbutton.n)
	self.halpin=halpin
	pycomp.newpin(halpin, HAL_BIT, HAL_OUT)
	changepin = halpin + ".changepin"
	self.changepin=changepin
	pycomp.newpin(changepin, HAL_BIT, HAL_IN)
        pycomp[self.changepin] = 0

	pyvcp_checkbutton.n += 1
		
        if initval >= 0.5:
            self.value=1
        else:
            self.value=0
        self.v.set(self.value)
        self.reset = 0
		
    def update(self,pycomp):
        # prevent race condition if connected to permanently on pin
	if pycomp[self.changepin] and not(self.reset):
	    self.v.set(not(self.v.get()))
	    self.reset = 1
    	    pycomp[self.changepin] = 0 # try to reset, but may not work
	    
	if not(pycomp[self.changepin]) and(self.reset):
    	    self.reset = 0 
    	    pycomp[self.changepin] = 0   # make sure is reset now
	
	pycomp[self.halpin]=self.v.get()




# -------------------------------------------





class pyvcp_button(Button):
    """ (control) a button 
        halpin is 1 when button pressed, 0 otherwise 
        optional halpin.disable disables the button
        <button>
            <halpin>"name"</halpin>
            <disablepin>True</disablepin>
        </button>"""
    n=0
    def __init__(self,master,pycomp,halpin=None,disable_pin=False,**kw):
        Button.__init__(self,master,**kw)
        if halpin == None:
            halpin = "button."+str(pyvcp_button.n)
            pyvcp_button.n += 1  
        self.halpin=halpin
        pycomp.newpin(halpin, HAL_BIT, HAL_OUT)
        self.disable_pin = disable_pin
        if not disable_pin == False:
            halpin_disable = halpin + ".disable" 
            pycomp.newpin(halpin_disable, HAL_BIT, HAL_IN)
            self.halpin_disable=halpin_disable      
        self.state=0;
        self.bind("<ButtonPress>", self.pressed)
        self.bind("<ButtonRelease>", self.released)    
        self.pycomp = pycomp

    def pressed(self,event):
        if self.disable_pin: 
            is_disabled = self.pycomp[self.halpin_disable] 
            if is_disabled == 1: return
        self.pycomp[self.halpin]=1

    def released(self,event):
        if self.disable_pin: 
            is_disabled = self.pycomp[self.halpin_disable] 
            if is_disabled == 1: return
        self.pycomp[self.halpin]=0

    def update(self,pycomp):
        if self.disable_pin: 
            is_disabled = pycomp[self.halpin_disable]     
            if is_disabled == 1: Button.config(self,state=DISABLED)
            else: Button.config(self,state=NORMAL)
        else:pass





# -------------------------------------------

class pyvcp_scale(Scale):
    """ (control) a slider 
        halpin-i is integer output 
        halpin-f is float output

        <scale>
            [ <halpin>"my-scale"</halpin> ]
            [ <resolution>0.1</resolution> ] scale value a whole number must end in '.'
            [ <orient>HORIZONTAL</orient>  ] aligns the scale horizontal
            [ <min_>-33</min_> ] sets the minimum value to -33
            [ <max_>26</max_> ] sets the maximum value to 26
            [ <initval>10</initval> ]  sets initial value to 10
            [ <param_pin>1</param_pin>] creates param pin if > 0, set to initval, value can then be set externally, ArcEye 2013
        </scale>

    """
    # FIXME scale resolution when ctrl/alt/shift is held down?
    # FIXME allow user to specify size
    n=0

    def __init__(self,master,pycomp,
                    resolution=1,halpin=None,halparam=None,min_=0,max_=10,initval=0,param_pin=0,**kw):
        self.resolution=resolution
        Scale.__init__(self,master,resolution=self.resolution,
                         from_=min_,to=max_,**kw)

        if halpin == None:
            halpin = "scale."+str(pyvcp_scale.n)
        self.halpin=halpin

        if halparam == None:
            self.param_pin = param_pin
            if self.param_pin == 1:
                halparam = "scale."+str(pyvcp_scale.n)+".param_pin"
                self.halparam=halparam        
                pycomp.newpin(halparam, HAL_FLOAT, HAL_IN)

        pyvcp_scale.n += 1       
        
        pycomp.newpin(halpin+"-i", HAL_S32, HAL_OUT)
        pycomp.newpin(halpin+"-f", HAL_FLOAT, HAL_OUT)
        
        self.bind('<Button-4>',self.wheel_up)
        self.bind('<Button-5>',self.wheel_down)
        

        if initval < min_:
            self.value=min_
        elif initval > max_:
            self.value=max_
        else:
            self.value=initval

        self.set(self.value)
        
        if self.param_pin == 1:       
            self.oldInit=self.value
            self.init=self.value
            pycomp[self.halparam] = self.value

    def update(self,pycomp):
        pycomp[self.halpin+"-f"]=self.get()
        pycomp[self.halpin+"-i"]=int(self.get())
        
        if self.param_pin == 1:
            self.init = pycomp[self.halparam]
            if self.init != self.oldInit :
                self.set(self.init)
                self.value=self.init
                self.oldInit=self.init

    def wheel_up(self,event):
        self.set(self.get()+self.resolution)

    def wheel_down(self,event):
        self.set(self.get()-self.resolution)


class pyvcp_table(Frame):
    """ Grid layout widget with rows and columns.
        * flexible_columns - list of column indexes that should be flexible width
        * flexible_rows - list of row indexes that should be flexible width
        * uniform_columns - string of characters for each column, in order. Columns with the same character will be the same width.
        * uniform_rows - string of characters for each row, in order. Rows with the same character will be the same height.
        * (also accepts Tk options for Frame)

        <table flexible_rows="[3]" flexible_columns="[3]" uniform_columns="aab" uniform_rows="aab" highlightthickness="10" highlightbackground="#CCCCCC">
            <tablesticky sticky="nwes"/>
            <!-- row 1 -->
            <tablerow/>
            <label text="AAAAAAAAA" />
            <label text="BBBBBBBBB" />
            <label text="CCCCCCCCC" />
            <!-- row 2 -->
            <tablerow/>
            <label text="A" />
            <label text="B" />
            <label text="C" />
            <!-- row 3 -->
            <tablerow/>
            <tablespan columns="3"/>
            <label text="Merged columns" />
        </table>
    """
    def __init__(self, master, pycomp, flexible_rows=[], flexible_columns=[], uniform_columns="", uniform_rows="", **kw):
        Frame.__init__(self, master, **kw)
        for r in flexible_rows:
            self.grid_rowconfigure(r, weight=1)
        for c in flexible_columns:
            self.grid_columnconfigure(c, weight=1)
        for i, r in enumerate(uniform_rows):
            self.grid_rowconfigure(i+1, uniform=r)
        for i, c in enumerate(uniform_columns):
            self.grid_columnconfigure(i+1, uniform=c)

        self._r = self._c = 0
        self.occupied = {}
        self.span = (1,1)
        self.sticky = "ne"

    def add(self, container, child):
        if isinstance(child, pyvcp_tablerow):
            self._r += 1
            self._c = 1
            return
        elif isinstance(child, pyvcp_tablespan):
            self.span = child.span
            return
        elif isinstance(child, pyvcp_tablesticky):
            self.sticky = child.sticky
            return
        r, c = self._r, self._c
        while self.occupied.has_key((r, c)):
            c = c + 1
        rs, cs = self.span
        child.grid(row=r, column=c, rowspan=rs, columnspan=cs,
                        sticky=self.sticky)
        for ri in range(r, r+rs):
            for ci in range(c, c+cs):
                self.occupied[ri,ci] = True

        self.span = 1,1
        self._c = c+cs

    def update(self, pycomp): pass

class pyvcp_tablerow:
    def __init__(self, master, pycomp): pass
    def update(self, pycomp): pass

class pyvcp_tablespan:
    def __init__(self, master, pycomp, rows=1, columns=1):
        self.span = rows, columns
    def update(self, pycomp): pass

class pyvcp_tablesticky:
    def __init__(self, master, pycomp, sticky):
        self.sticky = sticky
    def update(self, pycomp): pass
    
class pyvcp_include(Frame):
    def __init__(self, master, pycomp, src, expand="yes", fill="both", anchor="center", prefix=None, **kw):
        Frame.__init__(self,master,**kw)

        self.master = master
        self.fill = fill
        self.anchor = anchor
        self.expand = expand

        if prefix is not None:
            oldprefix = pycomp.getprefix()
            pycomp.setprefix(prefix)
        import vcpparse, xml.dom.minidom, xml.parsers.expat

        try:
            doc = xml.dom.minidom.parse(src) 
        except xml.parsers.expat.ExpatError, detail:
            print "Error: could not open",src,"!"
            print detail
            sys.exit(1)

        # find the pydoc element
        for e in doc.childNodes:
            if e.nodeType == e.ELEMENT_NODE and e.localName == "pyvcp":
                break

        if e.localName != "pyvcp":
            print "Error: no pyvcp element in file!"
            sys.exit()
        pyvcproot=e
        vcpparse.nodeiterator(pyvcproot,self)

        if prefix is not None:
            pycomp.setprefix(oldprefix)

    def update(self, pycomp): pass

    def add(self, container, widget):
        widget.pack(fill=self.fill, anchor=self.anchor, expand=self.expand)

class _pyvcp_dummy:
    def add(self, container, widget): pass
    def update(self, pycomp): pass
    def pack(self, *args, **kw): pass

class pyvcp_title(_pyvcp_dummy):
    def __init__(self, master, pycomp, title, iconname=None):
        master.wm_title(title)
        if iconname: master.wm_iconname(iconname)

class pyvcp_axisoptions(_pyvcp_dummy):
    def __init__(self, master, pycomp):
        import rs274.options
        rs274.options.install(master)

class pyvcp_option(_pyvcp_dummy):
    def __init__(self, master, pycomp, pattern, value, priority=None):
        master.option_add(pattern, value, priority)

class pyvcp_image(_pyvcp_dummy):
    all_images = {}
    def __init__(self, master, pycomp, name, **kw):
        self.all_images[name] = PhotoImage(name, kw, master)

class _pyvcp_image(Label):
    def __init__(self, master, pycomp, images, halpin=None, **kw):
        Label.__init__(self, master, **kw)
        if isinstance(images, basestring): images = images.split()
        self.images = images
        if halpin == None:
            halpin = "number."+str(pyvcp_number.n)
            pyvcp_number.n += 1
        self.halpin = halpin
        self.value = 0
        self.last = None
        pycomp.newpin(halpin, self.pintype, HAL_IN)

    def update(self, pycomp):
        l = pycomp[self.halpin]
        if l != self.last:
            try:
                self.configure(image=self.images[l])
            except (IndexError, KeyError):
                print >>sys.stderr, "Unknown image #%d on %s" % (l, self.halpin)
        self.last = l

class pyvcp_image_bit(_pyvcp_image):
    pintype = HAL_BIT
class pyvcp_image_u32(_pyvcp_image):
    pintype = HAL_U32

# This must come after all the pyvcp_xxx classes
elements = []
__all__ = []
for _key in globals().keys():
    if _key.startswith("pyvcp_"):
        elements.append(_key[6:])
        __all__.append(_key)

if __name__ == '__main__':
    print "You can't run pyvcp_widgets.py by itself..."
# vim:sts=4:sw=4:et:
