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



from Tkinter import *
from hal import *


# -------------------------------------------

class pyvcp_label(Label):
     " a static text label "
     def __init__(self,master,pycomp,**kw):
          Label.__init__(self,master,**kw)
     def update(self,pycomp):
          pass


# -------------------------------------------


class pyvcp_vbox(Frame):
     " a box in which widgets are packed vertically"
     def __init__(self,master,pycomp):
          Frame.__init__(self,master,bd=0,relief=FLAT)
     def update(self,pycomp): 
          pass
     def packtype(self):
          return "top"

# -------------------------------------------

class pyvcp_hbox(Frame):
     " a box in which widgets are packed horizontally"
     def __init__(self,master,pycomp):
          Frame.__init__(self,master,bd=0,relief=FLAT)
     def update(self,pycomp): 
          pass
     def packtype(self):
          return "left"


# -------------------------------------------


class pyvcp_spinbox(Spinbox):
     " (control) controls a float, also shown as text "
     " reacts to the mouse wheel "
     def __init__(self,master,pycomp,halpin="spinbox",
                    min_=0,max_=100,resolution=1,format="2.1f",**kw):
          self.v = DoubleVar()
          if 'increment' not in kw: kw['increment'] = resolution
          if 'from' not in kw: kw['from'] = min_
          if 'to' not in kw: kw['to'] = max_
          if 'format' not in kw: kw['format'] = "%" + format
          kw['command'] = self.command
          Spinbox.__init__(self,master,textvariable=self.v,**kw)
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
     " (indicator) shows a float as text "
     def __init__(self,master,pycomp,halpin="number",format="2.1f",**kw):
          self.v = StringVar()
          self.format=format
          Label.__init__(self,master,textvariable=self.v,**kw)
          self.halpin=halpin
          self.value=0.0
          dummy = "%(b)"+self.format
          self.v.set( str( dummy  % {'b':self.value} ) )
          #self.update(pycomp)
          pycomp.newpin(halpin, HAL_FLOAT, HAL_IN)

     def update(self,pycomp):    
          newvalue = pycomp[self.halpin]
          if newvalue != self.value:
               self.value=newvalue
               dummy = "%(b)"+self.format
               self.v.set( str( dummy  % {'b':newvalue} ) )

  

# -------------------------------------------

class pyvcp_bar(Canvas):
     " (indicator) a bar-indicator "
     def __init__(self,master,pycomp,
              fillcolor="green",bgcolor="grey",
               halpin="bar",startval=0.0,endval=100.0,**kw):
          self.cw=200    # canvas width
          self.ch=50     # canvas height
          self.bh=30     # bar height
          self.bw=150    # bar width
          self.pad=((self.cw-self.bw)/2)

          Canvas.__init__(self,master,width=self.cw,height=self.ch)
          self.halpin=halpin
          self.endval=endval
          self.startval=startval
          pycomp.newpin(halpin, HAL_FLOAT, HAL_IN)

          # the border
          border=self.create_rectangle(self.pad,1,self.pad+self.bw,self.bh)
          self.itemconfig(border,fill=bgcolor)

          # the bar
          self.bar=self.create_rectangle(self.pad+1,2,self.pad+1,self.bh-1)
          self.itemconfig(self.bar,fill=fillcolor)
          self.value=0.0 # some dummy value to start with     
          

          # start text
          start_text=self.create_text(self.pad,self.bh+10,text=str(startval) )

          #end text
          end_text=self.create_text(self.pad+self.bw,self.bh+10,text=str(endval) )
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
               self.coords(self.bar, self.pad+1, 2, self.pad+self.bw*percent, self.bh-1)






# -------------------------------------------




class pyvcp_led(Canvas):
     " (indicator) a LED "
     " color is on_color when halpin is 1, off_color when halpin is 0 "

     def __init__(self,master,pycomp,
                    halpin="led",off_color="red",on_color="green",size=20,**kw):
          Canvas.__init__(self,master,width=size,height=size,bd=0)
          self.off_color=off_color
          self.on_color=on_color
          self.oh=self.create_oval(1,1,size,size)
          self.state=0
          self.itemconfig(self.oh,fill=off_color)
          self.halpin=halpin
          pycomp.newpin(halpin, HAL_BIT, HAL_IN)

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
     " (control) a check button "
     " halpin is 1 when button checked, 0 otherwise "

     def __init__(self,master,pycomp,halpin="chbutton",**kw):
          self.v = BooleanVar(master)
          Checkbutton.__init__(self,master,variable=self.v,onvalue=1, offvalue=0,**kw)
          self.halpin=halpin
          pycomp.newpin(halpin, HAL_BIT, HAL_OUT)

     def update(self,pycomp):
          pycomp[self.halpin]=self.v.get()





# -------------------------------------------






class pyvcp_button(Button):
     " (control) a button "
     " halpin is 1 when button pressed, 0 otherwise "
     def __init__(self,master,pycomp,halpin="button",**kw):
          Button.__init__(self,master,**kw)
          self.halpin=halpin
          #p = component("test2");
          pycomp.newpin(halpin, HAL_BIT, HAL_OUT)
          self.state=0;
          self.bind("<ButtonPress>", self.pressed)
          self.bind("<ButtonRelease>", self.released)     

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
     " (control) a slider "
     " halpin-i is integer output "
     " halpin-f is float output "
     
     def __init__(self,master,pycomp,
                    resolution=1,halpin="scale",**kw):
          Scale.__init__(self,master,**kw)
          self.halpin=halpin
          self.resolution=resolution
          pycomp.newpin(halpin+"-i", HAL_S32, HAL_OUT)
          pycomp.newpin(halpin+"-f", HAL_FLOAT, HAL_OUT)
          self.bind('<Button-4>',self.wheel_up)
          self.bind('<Button-5>',self.wheel_down)

     def update(self,pycomp):
          pycomp[self.halpin+"-f"]=self.get()
          pycomp[self.halpin+"-i"]=int(self.get())

     def wheel_up(self,event):
          self.set(self.get()+self.resolution)

     def wheel_down(self,event):
          self.set(self.get()-self.resolution)

