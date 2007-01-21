#    This is a component of AXIS, a front-end for emc
#    Copyright 2007 Anders Wallin <anders.wallin@helsinki.fi>
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

# this makes only functions named here be included in the pydoc
__all__=["pyvcp_label"]


# FIXME: this is ugly, a list of valid widgets in this module should be 
# created somehow automagically
# or another mechanism for vcpparse.py to know which elements are valid should
# be constructed
elements =["pyvcp","led","vbox","hbox","vbox" \
            ,"button","scale","checkbutton","bar" \
            ,"label","number","spinbox","radiobutton","jogwheel"\
            ,"meter","dial"]

# FIXME: this is ugly, each widget should know what parameters are valid
# for itself, and vcpparse.py should check validity for each widget individually
parameters = ["size","text","orient","halpin","format" \
            ,"font","min_","max_","resolution" \
            ,"choices","cpr","fillcolor","bgcolor" \
            ,"bd","relief","init"]



from Tkinter import *
import math

# -------------------------------------------

class pyvcp_dial(Canvas):
    # Dial widget by tomp
    """ A dial that outputs a HAL_FLOAT 
        reacts to both mouse-wheel and mouse dragging
        <dial>
            [ <size>376</size> ]
                
            [ <cpr>100</cpr> ]    number of changes per rev, is # of dial tick marks, beware hi values)            
            [ <min_>-33.123456</min_> ]
            [ <max_>3.3</max_> ]
            [ <text>"Gallons per Hour"</text> ]            (knob label)
            [ <init>123</init> ]           (initial value a whole number must end in '.')
            [ <resolution>.001</resolution> ]          (scale value a whole number must end in '.')
            [ <halpin>"anaout"</halpin> ]
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
    
    def __init__(self,root,pycomp,halpin=None,size=200,cpr=40, \
            min_=None,max_=None, \
            text=None,init=0,resolution=0.1, \
            **kw):
        
        pad=10

        self.out=init                    #  float output   out   
        self.origValue=init       # in case user wants to reset the pot/valve/thingy

        #self.text3=resolution

        Canvas.__init__(self,root,width=size,height=size)
        self.circle=self.create_oval(pad,pad,size-pad,size-pad)

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
        self.itemconfig(self.dot,fill="yellow",activefill="green")

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
                        text=str(self.funit),font=('Arial',-self.txtroom))

        
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

        # create the hal pin
        if halpin == None:
            halpin = "dial."+str(pyvcp_dial.n)+".out"
        pyvcp_dial.n += 1
        pycomp.newpin(halpin, HAL_FLOAT, HAL_OUT)

        self.halpin=halpin
        self.pycomp=pycomp


    def chgScaleDn(self,event):
        # reduces the scale by 10x
        self.funit=self.funit/10.0
        self.update_scale()
    
    def chgScaleUp(self,event):
        # increases the scale by 10x
        self.funit=self.funit*10.0
        self.update_scale()
    
    def resetScale(self,event):
        # reset scale to original value
        self.funit=self.origFunit
        self.update_scale()
    
    def resetValue(self,event):
        # reset output to orifinal value
        self.out=self.origValue
        self.update_dro()

    def dot_coords(self):
        # calculate the coordinates for the dot
        DOTR=0.08*self.size
        DOTPOS=0.75
        midx = self.mid+DOTPOS*self.r*math.cos(self.alfa)
        midy = self.mid+DOTPOS*self.r*math.sin(self.alfa)
        return midx-DOTR, midy-DOTR,midx+DOTR,midy+DOTR

    def bdown(self,event):
        self.dragstartx=event.x
        self.dragstarty=event.y
        self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
        self.itemconfig(self.dot,fill="green",activefill="green")

    def bup(self,event):
        self.itemconfig(self.dot,fill="yellow")

    def motion(self,event):
        dragstop = math.atan2((event.y-self.mid),(event.x-self.mid))
        delta = dragstop - self.dragstart
        if delta>=self.d_alfa:
            self.up()
            self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
        elif delta<=-self.d_alfa:
            self.down()
            self.dragstart=math.atan2((event.y-self.mid),(event.x-self.mid))
        self.itemconfig(self.dot,fill="green",activefill="green")

    def wheel_up(self,event):
        self.up()

    def wheel_down(self,event):
        self.down()

    def down(self):
        self.alfa-=self.d_alfa
        self.out-=self.funit
        #TJP clip down side
        if self.mymin != None:
            if self.out<self.mymin:
                self.out=self.mymin
        self.update_dot()
        self.update_dro()

    def up(self):
        self.alfa+=self.d_alfa
        self.out+=self.funit
        #TJP clip up side
        if self.mymax != None:
            if self.out>self.mymax:
                self.out=self.mymax
        self.update_dot()
        self.update_dro()

    def update_dot(self):
        self.coords(self.dot, self.dot_coords() )

    def update_dro(self):
        valtext = str(self.out)
        self.itemconfig(self.dro,text=valtext)

    def update_scale(self):
        valtext = str(self.funit)
        valtext = 'x ' + valtext
        self.itemconfig(self.delta,text=valtext)

    def draw_ticks(self,cpr):
        for n in range(0,cpr):
            startx=self.mid+self.r*math.cos(n*self.d_alfa)
            starty=self.mid+self.r*math.sin(n*self.d_alfa)
            stopx=self.mid+1.15*self.r*math.cos(n*self.d_alfa)
            stopy=self.mid+1.15*self.r*math.sin(n*self.d_alfa)
            self.create_line([startx,starty,stopx,stopy])

    def update(self,pycomp):
        self.pycomp[self.halpin] = self.out




# -------------------------------------------

class pyvcp_meter(Canvas):
    """ Meter - shows the value of a FLOAT with an analog meter
        <meter>
            [ <size>300</size> ]
            [ <halpin>"mymeter"</halpin> ]
            [ <text>"My Voltage"</text> ]
            [ <min_>-22</min_> ]
            [ <max_>123</max_> ]
        </meter>
    """
    # FIXME: logarithmic scale option
    n=0
    def __init__(self,root,pycomp,halpin=None,
                        size=200,text=None,min_=0,max_=100,**kw):
        pad=10
        Canvas.__init__(self,root,width=size,height=size)
        self.halpin=halpin
        self.min_=min_
        self.max_=max_
        range_=2.5
        self.min_alfa=-math.pi/2-range_
        self.max_alfa=-math.pi/2+range_
        self.circle=self.create_oval(pad,pad,size-pad,size-pad)
        self.itemconfig(self.circle,fill="white")
        self.mid=size/2
        self.r=(size-2*pad)/2
        self.alfa=0
        self.line = self.create_line([self.mid,self.mid, \
                            self.mid+0.8*self.r*math.cos(self.alfa), \
                            self.mid+0.8*self.r*math.sin(self.alfa)],fill="red")
        self.itemconfig(self.line,width=3)
        if text!=None:
            t=self.create_text([self.mid,self.mid-20])
            self.itemconfig(t,text=text)
            self.itemconfig(t,font=('Arial',20))

        self.draw_ticks()

        # create the hal pin
        if halpin == None:
            self.halpin = "meter."+str(pyvcp_meter.n)+".value"
        pyvcp_meter.n += 1
        pycomp.newpin(self.halpin, HAL_FLOAT, HAL_IN)
        self.value = pycomp[self.halpin]

    def update(self,pycomp):
        self.value = pycomp[self.halpin]
        scale=(self.max_-self.min_)/(self.max_alfa-self.min_alfa)
        self.alfa=self.min_alfa + (self.value-self.min_)/scale
        if self.alfa > self.max_alfa:
            self.alfa = self.max_alfa
        elif self.alfa < self.min_alfa:
            self.alfa = self.min_alfa

        self.coords(self.line, self.mid,self.mid, \
                            self.mid+0.8*self.r*math.cos(self.alfa), \
                            self.mid+0.8*self.r*math.sin(self.alfa))   

    def draw_ticks(self):
        d_alfa = float((self.max_alfa-self.min_alfa))/10
        d_value = float((self.max_-self.min_))/10
        for n in range(0,11):
            startx=self.mid+self.r*math.cos(self.min_alfa + n*d_alfa)
            starty=self.mid+self.r*math.sin(self.min_alfa + n*d_alfa)
            stopx=self.mid+0.85*self.r*math.cos(self.min_alfa + n*d_alfa)
            stopy=self.mid+0.85*self.r*math.sin(self.min_alfa + n*d_alfa)
            textx=stopx - 0.1*self.r*math.cos(self.min_alfa + n*d_alfa)
            texty=stopy - 0.1*self.r*math.sin(self.min_alfa + n*d_alfa)
            self.create_line([startx,starty,stopx,stopy])
            t=self.create_text([textx,texty])
            self.itemconfig(t,text=str(self.min_+d_value*n))



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
        pad=10
        self.count=0
        Canvas.__init__(self,root,width=size,height=size)
        self.circle=self.create_oval(pad,pad,size-pad,size-pad)
        self.itemconfig(self.circle,fill="lightgrey",activefill="darkgrey")
        self.mid=size/2
        self.r=(size-2*pad)/2
        self.alfa=0
        self.d_alfa=2*math.pi/cpr
        self.size=size
        
        
        self.dot = self.create_oval(self.dot_coords())
        self.itemconfig(self.dot,fill="black")
        #self.itemconfig(self.line,arrow="last")
        #self.itemconfig(self.line,width=3)

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
        DOTR=0.08*self.size
        DOTPOS=0.75
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

class pyvcp_radiobutton(Frame):
    n=0
    def __init__(self,master,pycomp,halpin=None,choices=[],**kw):
        f=Frame.__init__(self,master,bd=2,relief=GROOVE)
        self.v = IntVar()
        self.v.set(1)
        self.choices=choices
        if halpin == None:
            halpin = "radiobutton."+str(pyvcp_radiobutton.n)
        pyvcp_radiobutton.n += 1
        
        self.halpins=[]
        n=0
        for c in choices:
            b=Radiobutton(self,f, text=str(c)
                        ,variable=self.v, value=pow(2,n))
            b.pack()
            if n==0:
                b.select()

            c_halpin=halpin+"."+str(c)
            pycomp.newpin(c_halpin, HAL_BIT, HAL_OUT)
            self.halpins.append(c_halpin)
            n+=1

    # FIXME
    # this is a fairly stupid way of updating the pins
    # since the calculation is done every 100ms wether a change
    # has happened or not. see below.   
    def update(self,pycomp):
        index=math.log(self.v.get(),2)
        index=int(index)
        for pin in self.halpins:
            pycomp[pin]=0;
        pycomp[self.halpins[index]]=1;

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
        </label>
    """
    def __init__(self,master,pycomp,**kw):
        Label.__init__(self,master,**kw)
        
    def update(self,pycomp):
        pass


# -------------------------------------------


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
    def update(self,pycomp): 
        pass
    def packtype(self):
        return "top"

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
    def update(self,pycomp): 
        pass
    def packtype(self):
        return "left"


# -------------------------------------------


class pyvcp_spinbox(Spinbox):
    """ (control) controls a float, also shown as text 
        reacts to the mouse wheel 
        <spinbox>
            [ <halpin>"my-spinbox"</halpin> ]
            [ <min_>55</min_> ]   sets the minimum value to 55
            [ <max_>123</max_> ]  sets the maximum value to 123
        </spinbox>
    """
    # FIXME: scale resolution when shift/ctrl/alt is held down?
    n=0
    def __init__(self,master,pycomp,halpin=None,
                    min_=0,max_=100,resolution=1,format="2.1f",**kw):
        self.v = DoubleVar()
        if 'increment' not in kw: kw['increment'] = resolution
        if 'from' not in kw: kw['from'] = min_
        if 'to' not in kw: kw['to'] = max_
        if 'format' not in kw: kw['format'] = "%" + format
        kw['command'] = self.command
        Spinbox.__init__(self,master,textvariable=self.v,**kw)
        if halpin == None:
            halpin = "spinbox."+str(pyvcp_spinbox.n)
        pyvcp_spinbox.n += 1
        self.halpin=halpin
        self.value=min_
        self.oldvalue=min_
        self.format = "%(b)"+format
        self.max_=max_
        self.min_=min_
        self.resolution=resolution
        self.v.set( str( self.format  % {'b':self.value} ) )
        pycomp.newpin(halpin, HAL_FLOAT, HAL_OUT)
        self.bind('<Button-4>',self.wheel_up)
        self.bind('<Button-5>',self.wheel_down)

    def command(self):
        self.value = self.v.get()

    def update(self,pycomp):  
        pycomp[self.halpin] = self.value 
        if self.value != self.oldvalue:
            self.v.set( str( self.format  % {'b':self.value} ) ) 
            self.oldvalue=self.value
          
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

  

# -------------------------------------------

class pyvcp_bar(Canvas):
    """ (indicator) a bar-indicator for a float"""
    n=0
    # FIXME logarithmic scale?
    # FIXME specifying negative startval or endval does not work
    def __init__(self,master,pycomp,
              fillcolor="green",bgcolor="grey",
               halpin=None,min_=0.0,max_=100.0,**kw):
    
        self.cw=200    # canvas width
        self.ch=50     # canvas height
        self.bh=30     # bar height
        self.bw=150    # bar width
        self.pad=((self.cw-self.bw)/2)

        Canvas.__init__(self,master,width=self.cw,height=self.ch)

        if halpin == None:
            halpin = "bar."+str(pyvcp_bar.n)
        pyvcp_bar.n += 1
        self.halpin=halpin
        self.endval=max_
        self.startval=min_
        pycomp.newpin(halpin, HAL_FLOAT, HAL_IN)

        # the border
        border=self.create_rectangle(self.pad,1,self.pad+self.bw,self.bh)
        self.itemconfig(border,fill=bgcolor)

        # the bar
        self.bar=self.create_rectangle(self.pad+1,2,self.pad+1,self.bh-1)
        self.itemconfig(self.bar,fill=fillcolor)
        self.value=0.0 # some dummy value to start with     
          
        # start text
        start_text=self.create_text(self.pad,self.bh+10,text=str(self.startval) )
        #end text
        end_text=self.create_text(self.pad+self.bw,self.bh+10,text=str(self.endval) )
        # value text
        self.val_text=self.create_text(self.pad+self.bw/2,
                                   self.bh/2,text=str(self.value) )
          
    def update(self,pycomp):
        # update value
        newvalue=pycomp[self.halpin]
        if newvalue != self.value:
            self.value = newvalue
            percent = self.value/(self.endval-self.startval)
            if percent < 0.0:
                percent = 0
            elif percent > 1.0:
                percent = 1.0  
            # set value text
            valtext = str( "%(b)3.1f" % {'b':self.value} )
            self.itemconfig(self.val_text,text=valtext)
            # set bar size
            self.coords(self.bar, self.pad+1, 2, 
                        self.pad+self.bw*percent, self.bh-1)






# -------------------------------------------




class pyvcp_led(Canvas):
    """ (indicator) a LED 
        color is on_color when halpin is 1, off_color when halpin is 0 """
    n=0
    def __init__(self,master,pycomp, halpin=None,      
                    off_color="red",on_color="green",size=20,**kw):
        Canvas.__init__(self,master,width=size,height=size,bd=0)
        self.off_color=off_color
        self.on_color=on_color
        self.oh=self.create_oval(1,1,size,size)
        self.state=0
        self.itemconfig(self.oh,fill=off_color)
        if halpin == None:
            halpin = "led."+str(pyvcp_led.n)
        
        self.halpin=halpin
        pycomp.newpin(halpin, HAL_BIT, HAL_IN)
        pyvcp_led.n+=1

    def update(self,pycomp):
        newstate = pycomp[self.halpin]
        if newstate != self.state:
            if newstate == 1:
                self.itemconfig(self.oh,fill=self.on_color)
                self.state=1
            else:
                self.itemconfig(self.oh,fill=self.off_color) 
                self.state=0






# -------------------------------------------






class pyvcp_checkbutton(Checkbutton):
    """ (control) a check button 
        halpin is 1 when button checked, 0 otherwise 
    """
    n=0
    def __init__(self,master,pycomp,halpin=None,**kw):
        self.v = BooleanVar(master)
        Checkbutton.__init__(self,master,variable=self.v,onvalue=1, offvalue=0,**kw)
        if halpin == None:
            halpin = "checkbutton."+str(pyvcp_checkbutton.n)
        self.halpin=halpin
        pycomp.newpin(halpin, HAL_BIT, HAL_OUT)
        pyvcp_checkbutton.n += 1

    def update(self,pycomp):
        pycomp[self.halpin]=self.v.get()





# -------------------------------------------






class pyvcp_button(Button):
    """ (control) a button 
        halpin is 1 when button pressed, 0 otherwise 
    """
    n=0
    def __init__(self,master,pycomp,halpin=None,**kw):
        Button.__init__(self,master,**kw)
        if halpin == None:
            halpin = "button."+str(pyvcp_button.n)
        self.halpin=halpin 
        pycomp.newpin(halpin, HAL_BIT, HAL_OUT)
        self.state=0;
        self.bind("<ButtonPress>", self.pressed)
        self.bind("<ButtonRelease>", self.released) 
        pyvcp_button.n += 1    

    def pressed(self,event):
        # "the button was pressed"
        self.state=1     

    def released(self,event):
        # the button was released
        self.state=0

    def update(self,pycomp):
        pycomp[self.halpin]=self.state





# -------------------------------------------




class pyvcp_scale(Scale):
    """ (control) a slider 
        halpin-i is integer output 
        halpin-f is float output 
    """
    # FIXME scale resolution when ctrl/alt/shift is held down?
    # FIXME allow user to specify size
    n=0
    def __init__(self,master,pycomp,
                    resolution=1,halpin=None,min_=0,max_=10,**kw):
        self.resolution=resolution
        Scale.__init__(self,master,resolution=self.resolution,
                         from_=min_,to=max_,**kw)
        if halpin == None:
            halpin = "scale."+str(pyvcp_scale.n)
        self.halpin=halpin
        
        pycomp.newpin(halpin+"-i", HAL_S32, HAL_OUT)
        pycomp.newpin(halpin+"-f", HAL_FLOAT, HAL_OUT)
        self.bind('<Button-4>',self.wheel_up)
        self.bind('<Button-5>',self.wheel_down)
        pyvcp_scale.n += 1

    def update(self,pycomp):
        pycomp[self.halpin+"-f"]=self.get()
        pycomp[self.halpin+"-i"]=int(self.get())

    def wheel_up(self,event):
        self.set(self.get()+self.resolution)

    def wheel_down(self,event):
        self.set(self.get()-self.resolution)

if __name__ == '__main__':
    print "You can't run pyvcp_widgets.py by itself..."
