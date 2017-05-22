#!/usr/bin/env python
#
#    This is stepconf, a graphical configuration editor for LinuxCNC
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#    stepconf 1.1 revamped by Chris Morley 2014
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
#
#************
# GUI PAGE
#************
import os
from stepconf.definitions import *

def gui_page_prepare(self):
    self.w.gui_pyvcp.set_active(self.d.pyvcp)
    self.on_gui_pyvcp_toggled()
    if  not self.w.createconfig.get_active():
        if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.d.machinename)):
            self.w.gui_rdo_pyvcp_custom.set_active(True)
    self.w.gui_select_axis.set_active(self.d.select_axis)
    self.w.gui_select_gmoccapy.set_active(self.d.select_gmoccapy)
    self.w.ladderconnect.set_active(self.d.ladderconnect)
    self.w.gui_pyvcpconnect.set_active(self.d.pyvcpconnect)
    # Gladevcp
    self.w.gui_gladevcp.set_active(self.d.gladevcp)
    self.w.spindlespeedbar.set_active(self.d.spindlespeedbar)
    self.w.spindleatspeed.set_active(self.d.spindleatspeed)
    self.w.gladevcpforcemax.set_active(self.d.gladevcpforcemax )
    self.w.centerembededgvcp.set_active(self.d.centerembededgvcp)
    self.w.sideembededgvcp.set_active(self.d.sideembededgvcp)
    self.w.standalonegvcp.set_active(self.d.standalonegvcp)
    self.w.gladevcpposition.set_active(self.d.gladevcpposition)
    self.w.gladevcpsize.set_active(self.d.gladevcpsize )
    self.w.autotouchz.set_active(self.d.autotouchz)
    """
    if os.path.exists(THEMEDIR):
        self.get_installed_themes()
    """

def gui_page_finish(self):
    SIG = self._p
    self.d.select_axis = self.w.gui_select_axis.get_active()
    self.d.select_gmoccapy = self.w.gui_select_gmoccapy.get_active()
    self.d.pyvcp = self.w.gui_pyvcp.get_active()
    self.d.pyvcpconnect = self.w.gui_pyvcpconnect.get_active()    
    if self.d.pyvcp == True:
        if self.w.gui_rdo_pyvcp_blank.get_active() == True:
            self.d.pyvcpname = "blank.xml"
            self.pyvcphaltype = 0
        if self.w.gui_rdo_pyvcp_spindle.get_active() == True:
            self.d.pyvcpname = "spindle.xml"
            self.d.pyvcphaltype = 1
        if self.w.gui_rdo_pyvcp_custom.get_active() == True:
            self.d.pyvcpname = "custompanel.xml"
        else:
            if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/custompanel.xml" % self.d.machinename)):
                if not self.a.warning_dialog(self._p.MESS_PYVCP_REWRITE,False):
                    return True
    # Gladevcp
    self.d.gladevcp = self.w.gui_gladevcp.get_active()        
    if self.d.gladevcp == True:
        if self.w.gui_rdo_gladevcp_blank.get_active() == True:
            self.d.gladevcpname = "blank.ui"
        if self.w.gui_rdo_default_display.get_active() == True:
            self.d.gladevcpname = "glade_default.ui"
        if self.w.gui_rdo_custom_galdevcp.get_active() == True:
            self.d.gladevcpname = "glade_custom.ui"
        else:
            if os.path.exists(os.path.expanduser("~/linuxcnc/configs/%s/glade_custom.ui" % self.d.machinename)):
                if not self.a.warning_dialog(self._p.MESS_GLADEVCP_REWRITE,False):
                    return True    


    self.d.spindlespeedbar = self.w.spindlespeedbar.get_active()
    self.d.spindleatspeed = self.w.spindleatspeed.get_active()
    self.d.gladevcpforcemax = self.w.gladevcpforcemax.get_active()
    self.d.centerembededgvcp = self.w.centerembededgvcp.get_active()
    self.d.sideembededgvcp = self.w.sideembededgvcp.get_active()
    self.d.standalonegvcp = self.w.standalonegvcp.get_active()
    self.d.gladevcpposition = self.w.gladevcpposition.get_active()
    self.d.gladevcpsize = self.w.gladevcpsize.get_active()
    self.d.autotouchz = self.w.autotouchz.get_active()
    
    self.d.maxspeeddisplay = self.w.maxspeeddisplay.get_value()
    self.d.gladevcpwidth = self.w.gladevcpwidth.get_value()
    self.d.gladevcpheight = self.w.gladevcpheight.get_value()
    self.d.gladevcpxpos = self.w.gladevcpxpos.get_value()
    self.d.gladevcpypos = self.w.gladevcpypos.get_value()   

    # set HALUI commands based on the user requested glade buttons
    hal_zerox = "G10 L20 P0 X0 ( Set X to zero )"
    hal_zeroy = "G10 L20 P0 Y0 ( Set Y to zero )"
    hal_zeroz = "G10 L20 P0 Z0 ( Set Z to zero )"
    hal_zeroa = "G10 L20 P0 A0 ( Set A to zero )"
    
    self.d.zerox = self.w.zerox.get_active()
    self.d.zeroy = self.w.zeroy.get_active()
    self.d.zeroz = self.w.zeroz.get_active()
    self.d.zeroa = self.w.zeroa.get_active()
    
    if  self.d.zerox:
        self.d.halui_list.append(hal_zerox)
    if  self.d.zeroy:
        self.d.halui_list.append(hal_zeroy)
    if  self.d.zeroz:
        self.d.halui_list.append(hal_zeroz)
    if  self.d.zeroa:
        self.d.halui_list.append(hal_zeroa)
        
    if(self.d.zerox or self.d.zeroy or self.d.zeroz or self.d.zeroa):
        self.d.halui = 1
        
    #self.d.gladevcptheme = self.w.gladevcptheme.get_active_text()
    # make sure there is a copy of the choosen gladevcp panel in /tmp/
    # We will copy it later into our config folder
    self.gladevcptestpanel(self)
    if self.w.autotouchz.get_active():
        self.d.classicladder = True
        if not self.w.ladderexist.get_active():
            self.w.laddertouchz.set_active(True)

def on_show_gladepvc_clicked(self,*args):
    self.test_glade_panel(self)

def on_show_pyvcp_clicked(self,*args):
    self.testpanel(self)		

def on_gui_gladevcp_toggled(self,*args):
    # TODO
    i= self.w.gui_gladevcp.get_active()
    if  self.w.createconfig.get_active():
        self.w.gui_rdo_custom_galdevcp.set_sensitive(False)
    else:
        self.w.gui_rdo_custom_galdevcp.set_sensitive(i)
    self.w.gui_gladevcp_box.set_sensitive(i) 

def on_gui_pyvcp_toggled(self,*args):
    i= self.w.gui_pyvcp.get_active()
    if  self.w.createconfig.get_active():
        self.w.gui_rdo_pyvcp_custom.set_sensitive(False)
    else:
        self.w.gui_rdo_pyvcp_custom.set_sensitive(i)
    self.w.gui_pyvcp_box.set_sensitive(i)

"""
def get_installed_themes(self):
    data1 = self.d.gladevcptheme
    data2 = prefs.getpref('gtk_theme', 'Follow System Theme', str)
    model = self.widgets.themestore
    model.clear()
    model.append(("Follow System Theme",))
    temp1 = temp2 = 0
    names = os.listdir(_PD.THEMEDIR)
    names.sort()
    for search,dirs in enumerate(names):
        model.append((dirs,))
        if dirs  == data1:
            temp1 = search+1
        if dirs  == data2:
            temp2 = search+1
    self.widgets.gladevcptheme.set_active(temp1)
    self.widgets.touchytheme.set_active(temp2)
"""

#***************
# GLADEVCP TEST
#***************
def test_glade_panel(self,w):
    panelname = os.path.join(self.a.distdir, "configurable_options/gladevcp")
    if self.w.gui_rdo_gladevcp_blank.get_active() == True:
        print 'no sample requested'
        return True
    if self.w.gui_rdo_default_display.get_active() == True:
        self.gladevcptestpanel(w)
        self.display_gladevcp_panel()
    if self.w.gui_rdo_custom_galdevcp.get_active() == True:
        None
	
#***************
# PYVCP TEST
#***************
def testpanel(self,w):
    panelname = os.path.join(self.a.distdir, "configurable_options/pyvcp")
    if self.w.gui_rdo_pyvcp_blank.get_active() == True:
        print 'no sample requested'
        return True
    if self.w.gui_rdo_pyvcp_spindle.get_active() == True:
        panel = "default_panel.glade"
    if self.w.gui_rdo_pyvcp_custom.get_active() == True:
        panel = "custompanel.xml"
        panelname = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
        halrun = os.popen("cd %(panelname)s\nhalrun -Is > /dev/null"% {'panelname':panelname,}, "w" )    
        halrun.write("loadusr -Wn displaytest pyvcp -c displaytest %(panel)s\n" %{'panel':panel,})
    if self.w.gui_rdo_pyvcp_spindle.get_active() == True:
        halrun.write("setp displaytest.spindle-speed 1000\n")
    halrun.write("waitusr displaytest\n")
    halrun.flush()
    halrun.close()

def display_gladevcp_panel(self):
        pos = "+0+0"
        size = "200x200"
        options = ""
        folder = "/tmp"
        if not self.w.createconfig.get_active() and self.w.gladeexists.get_active():
            folder = os.path.expanduser("~/linuxcnc/configs/%s" % self.d.machinename)
            if not os.path.exists(folder + "/gvcp-panel.ui"):
                self.a.warning_dialog (_("""You specified there is an existing gladefile, \
But there is not one in the machine-named folder.."""),True)
                return
        self.gladevcptestpanel(self)
        if self.w.gladevcpposition.get_active() == True:
            xpos = self.w.gladevcpxpos.get_value()
            ypos = self.w.gladevcpypos.get_value()
            pos = "+%d+%d"% (xpos,ypos)
        if self.w.gladevcpsize.get_active() == True:
            width = self.w.gladevcpwidth.get_value()
            height = self.w.gladevcpheight.get_value()
            size = "%dx%d"% (width,height)
        """
        if not self.w.gladevcptheme.get_active_text() == "Follow System Theme":
            options ="-t %s"% (self.w.gladevcptheme.get_active_text())
            print options
        """
        halrun = os.popen("cd %s\nhalrun -Is > /dev/null"%(folder), "w" )
        if self.a.debug:
            halrun.write("echo\n")
        halrun.write("loadusr -Wn displaytest gladevcp -g %(size)s%(pos)s -c displaytest %(option)s gvcp-panel.ui\n" %{
                        'size':size,'pos':pos,'option':options})
        if self.w.spindlespeedbar.get_active():
            halrun.write("setp displaytest.spindle-speed 500\n")
        if self.w.zerox.get_active():
            halrun.write("setp displaytest.zero-x-active true\n")
        if self.w.zeroy.get_active():
            halrun.write("setp displaytest.zero-y-active true\n")
        if self.w.zeroz.get_active():
            halrun.write("setp displaytest.zero-z-active true\n")
        if self.w.zeroa.get_active():
            halrun.write("setp displaytest.zero-a-active true\n")
        if self.w.autotouchz.get_active():
            halrun.write("setp displaytest.auto-touch-z-active true\n")
        if self.w.spindleatspeed.get_active():
            halrun.write("setp displaytest.spindle-at-speed-led true\n")
        halrun.write("setp displaytest.button-box-active true\n")
        halrun.write("waitusr displaytest\n")
        halrun.flush()
        halrun.close()

def gladevcptestpanel(self,w):
    directory = "/tmp/"
    filename = os.path.join(directory, "gvcp-panel.ui")
    file = open(filename, "w")
    print >>file, ("""<?xml version="1.0"?>
<interface>
  <!-- interface-requires gladevcp 0.0 -->
  <requires lib="gtk+" version="2.16"/>
  <!-- interface-naming-policy project-wide -->
  <object class="GtkWindow" id="window1">
    <property name="width_request">100</property>
    <child>
      <object class="GtkVBox" id="vbox1">
        <property name="visible">True</property>""")
    if self.w.spindlespeedbar.get_active():
        print >>file, ("""
        <child>
          <object class="HAL_HBar" id="spindle-speed">
            <property name="visible">True</property>
            <property name="force_height">36</property>""")
        print >>file, ("""<property name="max">%(maxrpm)d</property>"""%{'maxrpm':self.w.maxspeeddisplay.get_value() })
        print >>file, ("""
            <property name="z0_color">#0000ffff0000</property>
            <property name="value">44.25</property>
            <property name="z1_color">#ffffffff0000</property>
            <property name="bg_color">#bebebebebebe</property>
            <property name="text_template">Spindle: % 4d RPM</property>
            <property name="z0_border">0.94999998807907104</property>
            <property name="z2_color">#ffff00000000</property>
            <property name="show_limits">False</property>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="position">0</property>
          </packing>
        </child>""" )
    if self.w.spindleatspeed.get_active():
        print >>file, ("""
        <child>
          <object class="GtkHBox" id="hbox1">
            <property name="visible">True</property>
            <child>
              <object class="GtkLabel" id="label1">
                <property name="visible">True</property>
                <property name="ypad">5</property>
                <property name="label" translatable="yes"> Spindle Up To Speed </property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">False</property>
                <property name="position">0</property>
              </packing>
            </child>
            <child>
              <object class="HAL_LED" id="spindle-at-speed-led">
                <property name="visible">True</property>
                <property name="led_shape">2</property>
                <property name="on_color">green</property>
                <property name="led_size">5</property>
              </object>
              <packing>
                <property name="expand">False</property>
                <property name="fill">False</property>
                <property name="padding">10</property>
                <property name="position">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="position">1</property>
          </packing>
        </child>""")
    print >>file, ("""
        <child>
          <object class="HAL_Table" id="button-box-active">
            <property name="visible">True</property>
            <property name="n_rows">5</property>
            <property name="homogeneous">False</property>""")
    if self.w.autotouchz.get_active():
        print >>file, ("""
            <child>
              <object class="HAL_HBox" id="auto-touch-z-active">
                <property name="visible">True</property>
                <child>
                  <object class="HAL_Button" id="auto-touch-z">
                    <property name="label" translatable="yes">Z  Auto Touch Off</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                    <property name="yalign">0.56000000238418579</property>
                  </object>
                  <packing>
                    <property name="position">0</property>
                  </packing>
                </child>
              </object>
              <packing>
                <property name="top_attach">4</property>
                <property name="bottom_attach">5</property>
              </packing>
            </child>""")
    if self.w.zeroa.get_active():
        print >>file, ("""
            <child>
              <object class="HAL_HBox" id="zero-a-active">
                <property name="visible">True</property>
                <child>
                  <object class="HAL_Button" id="zero-a">
                    <property name="label" translatable="yes">Zero A</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                  </object>
                  <packing>
                    <property name="position">0</property>
                  </packing>
                </child>
              </object>
              <packing>
                <property name="top_attach">3</property>
                <property name="bottom_attach">4</property>
              </packing>
            </child>""")
    if self.w.zeroz.get_active():
        print >>file, ("""
            <child>
              <object class="HAL_HBox" id="zero-z-active">
                <property name="visible">True</property>
                <child>
                  <object class="HAL_Button" id="zero-z">
                    <property name="label" translatable="yes">Zero Z</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                  </object>
                  <packing>
                    <property name="position">0</property>
                  </packing>
                </child>
              </object>
              <packing>
                <property name="top_attach">2</property>
                <property name="bottom_attach">3</property>
              </packing>
            </child>""")
    if self.w.zeroy.get_active():
        print >>file, ("""
            <child>
              <object class="HAL_HBox" id="zero-y-active">
                <property name="visible">True</property>
                <child>
                  <object class="HAL_Button" id="zero-y">
                    <property name="label" translatable="yes">Zero Y</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                  </object>
                  <packing>
                    <property name="position">0</property>
                  </packing>
                </child>
              </object>
              <packing>
                <property name="top_attach">1</property>
                <property name="bottom_attach">2</property>
              </packing>
            </child>""")
    if self.w.zerox.get_active():
        print >>file, ("""
            <child>
              <object class="HAL_HBox" id="zero-x-active">
                <property name="visible">True</property>
                <child>
                  <object class="HAL_Button" id="zero-x">
                    <property name="label" translatable="yes">Zero X</property>
                    <property name="visible">True</property>
                    <property name="can_focus">True</property>
                    <property name="receives_default">True</property>
                  </object>
                  <packing>
                    <property name="position">0</property>
                  </packing>
                </child>
              </object>
            </child>""")
    print >>file, ("""
          </object>
          <packing>
            <property name="expand">False</property>
            <property name="fill">False</property>
            <property name="position">2</property>
          </packing>
        </child>
      </object>
    </child>
  </object>
</interface>""")
    file.close()
