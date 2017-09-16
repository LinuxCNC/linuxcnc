#!/usr/bin/env python
# -*- encoding: utf-8 -*-
#
#    This is import_mach, a program to convert a mach 3 xml file to stepconf
#    xml file. Up to 4 axis can be converted.
#    Copyright 2015 John Thornton <jt@gnipsel.com>
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
#

from xml.dom.minidom import parseString
import sys
import os
import gtk

filter = gtk.FileFilter()
filter.add_pattern("*.xml")
filter.set_name(_("Mach configuration files"))
fcd = gtk.FileChooserDialog("Open...",
           None,
           gtk.FILE_CHOOSER_ACTION_OPEN,
           (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
fcd.set_current_folder(os.path.expanduser('~/Desktop'))
fcd.add_filter(filter)
response = fcd.run()
if response == gtk.RESPONSE_OK:
  file_name =  fcd.get_filename()
  machinename = os.path.splitext(os.path.basename(fcd.get_filename()))[0]
fcd.destroy()
if response == gtk.RESPONSE_CANCEL:
  quit(1)
print file_name
print machinename
file = open(file_name,'r')
#convert to string:
data = file.read()
#close file because we dont need it anymore:
file.close()
#parse the xml you got from the file
dom = parseString(data)

#quit()

'''
if (len(sys.argv) > 1):
  #open the xml file for reading:
  file = open(sys.argv[1],'r')
  #convert to string:
  data = file.read()
  #close file because we dont need it anymore:
  file.close()
  #parse the xml you got from the file
  dom = parseString(data)
else:
  # exit program if no file name is passed
  sys.exit("Usage is mark.py filename configname")

if (len(sys.argv) > 2):
  machinename = sys.argv[2]
else:
  xmlTag = dom.getElementsByTagName('Profile')[0].toxml()
  machinename =xmlTag.replace('<Profile>','').replace('</Profile>','')
'''

try:
  xmlTag = dom.getElementsByTagName('Units')[0].toxml()
  units =xmlTag.replace('<Units>','').replace('</Units>','')
  if units == "0":
    units = "1"
  else:
    units = "0"
except IndexError:
  units = "1"

# create the pins dictionary
dPins = {'1':'unused-output', '2':'unused-output', '2':'unused-output', '3':'unused-output', '4':'unused-output', '5':'unused-output', '6':'unused-output', '7':'unused-output', '8':'unused-output', '9':'unused-output', '10':'unused-input', '11':'unused-input', '12':'unused-input', '13':'unused-input', '14':'unused-output', '15':'unused-input', '16':'unused-output', '17':'unused-output'}

# create the pin invert dictionary
dInvert = {'1':'False', '2':'False', '2':'False', '3':'False', '4':'False', '5':'False', '6':'False', '7':'False', '8':'False', '9':'False', '10':'False', '11':'False', '12':'False', '13':'False', '14':'False', '15':'False', '16':'False', '17':'False'}

errors = ''

# X Axis
xmlTag = dom.getElementsByTagName('Motor0Active')[0].toxml()
xActive=xmlTag.replace('<Motor0Active>','').replace('</Motor0Active>','')
if xActive == '1':
  xmlTag = dom.getElementsByTagName('Motor0StepPin')[0].toxml()
  xStepPin = xmlTag.replace('<Motor0StepPin>','').replace('</Motor0StepPin>','')
  dPins[xStepPin] = 'xstep'
  xmlTag = dom.getElementsByTagName('Motor0DirPin')[0].toxml()
  xDirPin = xmlTag.replace('<Motor0DirPin>','').replace('</Motor0DirPin>','')
  dPins[xDirPin] = 'xdir'
  xmlTag = dom.getElementsByTagName('Motor0StepNeg')[0].toxml()
  xStepInvert = xmlTag.replace('<Motor0StepNeg>','').replace('</Motor0StepNeg>','')
  if xStepInvert == "1":
    dInvert[xStepPin] = 'True'
  xmlTag = dom.getElementsByTagName('Motor0DirNeg')[0].toxml()
  xDirInvert = xmlTag.replace('<Motor0DirNeg>','').replace('</Motor0DirNeg>','')
  if xDirInvert == "1":
    dInvert[xDirPin] = 'True'
  xmlTag = dom.getElementsByTagName('M0Max')[0].toxml()
  xmaxlim = float(str(xmlTag.replace('<M0Max>','').replace('</M0Max>','')))
  xmlTag = dom.getElementsByTagName('M0Min')[0].toxml()
  xminlim = float(str(xmlTag.replace('<M0Min>','').replace('</M0Min>','')))
  try:
    xmlTag = dom.getElementsByTagName('Steps0')[0].toxml()
    xscale = float(xmlTag.replace('<Steps0>','').replace('</Steps0>',''))
    xleadscrew = float(xscale / 200)
  except IndexError:
    errors +=  "No X Axis Scale was found!\n"
    xleadscrew = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Vel0')[0].toxml()
    xmaxvel = float(xmlTag.replace('<Vel0>','').replace('</Vel0>',''))
  except IndexError:
    errors +=  "No X Axis Max Velocity was found!\n"
    xmaxvel = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Acc0')[0].toxml()
    xmaxacc = float(xmlTag.replace('<Acc0>','').replace('</Acc0>',''))
  except IndexError:
    errors +=  "No X Axis Acceleration was found!\n"
    xmaxacc = 0.0
else:
  xmaxlim = 0.0
  xminlim = 0.0
  xleadscrew = 1.0
  xmaxvel = 0.0
  xmaxacc = 0.0

# Y Axis
xmlTag = dom.getElementsByTagName('Motor1Active')[0].toxml()
yActive=xmlTag.replace('<Motor1Active>','').replace('</Motor1Active>','')
if yActive == '1':
  xmlTag = dom.getElementsByTagName('Motor1StepPin')[0].toxml()
  yStepPin = xmlTag.replace('<Motor1StepPin>','').replace('</Motor1StepPin>','')
  dPins[yStepPin] = 'ystep'
  xmlTag = dom.getElementsByTagName('Motor1DirPin')[0].toxml()
  yDirPin = xmlTag.replace('<Motor1DirPin>','').replace('</Motor1DirPin>','')
  dPins[yDirPin] = 'ydir'
  xmlTag = dom.getElementsByTagName('Motor1StepNeg')[0].toxml()
  yStepInvert = xmlTag.replace('<Motor1StepNeg>','').replace('</Motor1StepNeg>','')
  if yStepInvert == "1":
    dInvert[yStepPin] = 'True'
  xmlTag = dom.getElementsByTagName('Motor1DirNeg')[0].toxml()
  yDirInvert = xmlTag.replace('<Motor1DirNeg>','').replace('</Motor1DirNeg>','')
  if yDirInvert == "1":
    dInvert[yDirPin] = 'True'
  xmlTag = dom.getElementsByTagName('M1Max')[0].toxml()
  ymaxlim = float(str(xmlTag.replace('<M1Max>','').replace('</M1Max>','')))
  xmlTag = dom.getElementsByTagName('M1Min')[0].toxml()
  yminlim = float(str(xmlTag.replace('<M1Min>','').replace('</M1Min>','')))
  try:
    xmlTag = dom.getElementsByTagName('Steps1')[0].toxml()
    yscale = float(xmlTag.replace('<Steps1>','').replace('</Steps1>',''))
    yleadscrew = float(yscale / 200)
  except IndexError:
    errors +=  "No Y Axis Scale was found!\n"
    yleadscrew = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Vel1')[0].toxml()
    ymaxvel = float(xmlTag.replace('<Vel1>','').replace('</Vel1>',''))
  except IndexError:
    errors +=  "No Y Axis Max Velocity was found!\n"
    ymaxvel = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Acc1')[0].toxml()
    ymaxacc = float(xmlTag.replace('<Acc1>','').replace('</Acc1>',''))
  except IndexError:
    errors +=  "No Y Axis Acceleration was found!\n"
    ymaxacc = 0.0
else:
  ymaxlim = 0.0
  yminlim = 0.0
  yleadscrew = 1.0
  ymaxvel = 0.0
  ymaxacc = 0.0

# Z Axis
xmlTag = dom.getElementsByTagName('Motor2Active')[0].toxml()
zActive=xmlTag.replace('<Motor2Active>','').replace('</Motor2Active>','')
if zActive == '1':
  xmlTag = dom.getElementsByTagName('Motor2StepPin')[0].toxml()
  zStepPin = xmlTag.replace('<Motor2StepPin>','').replace('</Motor2StepPin>','')
  dPins[zStepPin] = 'zstep'
  xmlTag = dom.getElementsByTagName('Motor2DirPin')[0].toxml()
  zDirPin = xmlTag.replace('<Motor2DirPin>','').replace('</Motor2DirPin>','')
  dPins[zDirPin] = 'zdir'
  xmlTag = dom.getElementsByTagName('Motor2StepNeg')[0].toxml()
  zStepInvert = xmlTag.replace('<Motor2StepNeg>','').replace('</Motor2StepNeg>','')
  if zStepInvert == "1":
    dInvert[zStepPin] = 'True'
  xmlTag = dom.getElementsByTagName('Motor2DirNeg')[0].toxml()
  zDirInvert = xmlTag.replace('<Motor2DirNeg>','').replace('</Motor2DirNeg>','')
  if zDirInvert == "1":
    dInvert[zStepPin] = 'True'
  xmlTag = dom.getElementsByTagName('M2Max')[0].toxml()
  zmaxlim = float(str(xmlTag.replace('<M2Max>','').replace('</M2Max>','')))
  xmlTag = dom.getElementsByTagName('M2Min')[0].toxml()
  zminlim = float(str(xmlTag.replace('<M2Min>','').replace('</M2Min>','')))
  try:
    xmlTag = dom.getElementsByTagName('Steps2')[0].toxml()
    zscale = float(xmlTag.replace('<Steps2>','').replace('</Steps2>',''))
    zleadscrew = float(zscale / 200)
  except IndexError:
    errors +=  "No Z Axis Scale was found!\n"
    zleadscrew = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Vel2')[0].toxml()
    zmaxvel = float(xmlTag.replace('<Vel2>','').replace('</Vel2>',''))
  except IndexError:
    errors +=  "No Z Axis Max Velocity was found!\n"
    zmaxvel = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Acc2')[0].toxml()
    zmaxacc = float(xmlTag.replace('<Acc2>','').replace('</Acc2>',''))
  except IndexError:
    errors +=  "No Z Axis Acceleration was found!\n"
    zmaxacc = 0.0
else:
  zmaxlim = 0.0
  zmaxlim = 0.0
  zleadscrew = 1.0
  zmaxvel = 0.0
  zmaxacc = 0.0

# A Axis
xmlTag = dom.getElementsByTagName('Motor3Active')[0].toxml()
aActive=xmlTag.replace('<Motor3Active>','').replace('</Motor3Active>','')
if aActive == '1':
  xmlTag = dom.getElementsByTagName('Motor3StepPin')[0].toxml()
  aStepPin = xmlTag.replace('<Motor3StepPin>','').replace('</Motor3StepPin>','')
  dPins[aStepPin] = 'astep'
  xmlTag = dom.getElementsByTagName('Motor3DirPin')[0].toxml()
  aDirPin = xmlTag.replace('<Motor3DirPin>','').replace('</Motor3DirPin>','')
  dPins[aDirPin] = 'adir'
  xmlTag = dom.getElementsByTagName('Motor3StepNeg')[0].toxml()
  aStepInvert = xmlTag.replace('<Motor3StepNeg>','').replace('</Motor3StepNeg>','')
  if aStepInvert == "1":
    dInvert[aStepPin] = 'True'
  xmlTag = dom.getElementsByTagName('Motor3DirNeg')[0].toxml()
  aDirInvert = xmlTag.replace('<Motor3DirNeg>','').replace('</Motor3DirNeg>','')
  if aDirInvert == "1":
    dInvert[aDirPin] = 'True'
  xmlTag = dom.getElementsByTagName('M3Max')[0].toxml()
  amaxlim = float(str(xmlTag.replace('<M3Max>','').replace('</M3Max>','')).rstrip('.'))
  xmlTag = dom.getElementsByTagName('M3Min')[0].toxml()
  aminlim = float(str(xmlTag.replace('<M3Min>','').replace('</M3Min>','')).rstrip('.'))
  try:
    xmlTag = dom.getElementsByTagName('Steps3')[0].toxml()
    ascale = float(xmlTag.replace('<Steps3>','').replace('</Steps3>',''))
    aleadscrew = float(ascale / 200)
  except IndexError:
    errors +=  "No A Axis Scale was found!\n"
    aleadscrew = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Vel3')[0].toxml()
    amaxvel = str(xmlTag.replace('<Vel3>','').replace('</Vel3>',''))
    amaxvel = amaxvel.rsplit(".",1)
    amaxvel = float(amaxvel[0])
  except IndexError:
    errors +=  "No A Axis Max Velocity was found!\n"
    amaxvel = 0.0
  try:
    xmlTag = dom.getElementsByTagName('Acc3')[0].toxml()
    amaxacc = str(xmlTag.replace('<Acc3>','').replace('</Acc3>',''))
    amaxacc = amaxacc.rsplit(".",1)
    amaxacc = float(amaxacc[0])
  except IndexError:
    errors +=  "No A Axis Acceleration was found!\n"
    amaxacc = 0.0
else:
  amaxlim = 0.0
  aminlim = 0.0
  aleadscrew = 1.0
  amaxvel = 0.0
  amaxacc = 0.0


# open the stepconf file for writing
filename = "/tmp/temp.stepconf"
sc = open(filename, "wb")
sc.write('<?xml version="1.0" ?>\n')
sc.write("<stepconf>\n")
sc.write('  <property name="ahomepos" type="int" value="0"/>\n')
sc.write('  <property name="ahomesw" type="int" value="0"/>\n')
sc.write('  <property name="ahomevel" type="float" value="0"/>\n')
sc.write('  <property name="alatchdir" type="int" value="0"/>\n')
sc.write('  <property name="aleadscrew" type="float" value="%f"/>\n'%aleadscrew)
sc.write('  <property name="amaxacc" type="float" value="%f"/>\n'% amaxacc)
sc.write('  <property name="amaxlim" type="float" value="%f"/>\n'% amaxlim)
sc.write('  <property name="amaxvel" type="float" value="%s"/>\n'% amaxvel)
sc.write('  <property name="amicrostep" type="int" value="1"/>\n')
sc.write('  <property name="aminlim" type="float" value="%f"/>\n'% aminlim)
sc.write('  <property name="apulleyden" type="int" value="1"/>\n')
sc.write('  <property name="apulleynum" type="int" value="1"/>\n')
sc.write('  <property name="ascale" type="int" value="0"/>\n')
sc.write('  <property name="asteprev" type="int" value="200"/>\n')
if aActive == '1': axes = 1 # rotary 
elif yActive == '0': axes = 2 # lathe
else: axes = 0 # XYZ
sc.write('  <property name="axes" type="int" value="%d"/>\n'% axes)
sc.write('  <property name="classicladder" type="bool" value="False"/>\n')
sc.write('  <property name="createshortcut" type="bool" value="False"/>\n')
sc.write('  <property name="createsymlink" type="bool" value="False"/>\n')
sc.write('  <property name="customhal" type="int" value="1"/>\n')
sc.write('  <property name="digitsin" type="float" value="15.0"/>\n')
sc.write('  <property name="digitsout" type="float" value="15.0"/>\n')
sc.write('  <property name="dirhold" type="float" value="20000.0"/>\n')
sc.write('  <property name="dirsetup" type="float" value="20000.0"/>\n')
sc.write('  <property name="drivertype" type="string" value="other"/>\n')
sc.write('  <property name="floatsin" type="float" value="10.0"/>\n')
sc.write('  <property name="floatsout" type="float" value="10.0"/>\n')
sc.write('  <property name="halui" type="bool" value="False"/>\n')
sc.write('  <property name="ioaddr" type="string" value="0"/>\n')
sc.write('  <property name="ioaddr2" type="string" value="1"/>\n')
sc.write('  <property name="ioaddr3" type="string" value="Enter Address"/>\n')
sc.write('  <property name="ladderconnect" type="bool" value="True"/>\n')
sc.write('  <property name="ladderhaltype" type="int" value="0"/>\n')
sc.write('  <property name="laddername" type="string" value="custom.clp"/>\n')
sc.write('  <property name="latency" type="float" value="15000.0"/>\n')
sc.write('  <property name="machinename" type="string" value="' + machinename + '"/>\n')
sc.write('  <property name="manualtoolchange" type="bool" value="True"/>\n')
sc.write('  <property name="md5sums" type="eval" value="[]"/>\n')
sc.write('  <property name="modbus" type="bool" value="False"/>\n')
sc.write('  <property name="number_pports" type="int" value="1"/>\n')
sc.write('  <property name="period" type="int" value="25000"/>\n')
sc.write('  <property name="pin1" type="string" value="' + dPins['1'] + '"/>\n')
sc.write('  <property name="pin10" type="string" value="' + dPins['10'] + '"/>\n')
sc.write('  <property name="pin10inv" type="bool" value="' + dInvert['10'] + '"/>\n')
sc.write('  <property name="pin11" type="string" value="' + dPins['11'] + '"/>\n')
sc.write('  <property name="pin11inv" type="bool" value="' + dInvert['11'] + '"/>\n')
sc.write('  <property name="pin12" type="string" value="' + dPins['12'] + '"/>\n')
sc.write('  <property name="pin12inv" type="bool" value="' + dInvert['12'] + '"/>\n')
sc.write('  <property name="pin13" type="string" value="' + dPins['13'] + '"/>\n')
sc.write('  <property name="pin13inv" type="bool" value="' + dInvert['13'] + '"/>\n')
sc.write('  <property name="pin14" type="string" value="' + dPins['14'] + '"/>\n')
sc.write('  <property name="pin14inv" type="bool" value="' + dInvert['14'] + '"/>\n')
sc.write('  <property name="pin15" type="string" value="' + dPins['15'] + '"/>\n')
sc.write('  <property name="pin15inv" type="bool" value="' + dInvert['15'] + '"/>\n')
sc.write('  <property name="pin16" type="string" value="' + dPins['16'] + '"/>\n')
sc.write('  <property name="pin16inv" type="bool" value="' + dInvert['16'] + '"/>\n')
sc.write('  <property name="pin17" type="string" value="' + dPins['17'] + '"/>\n')
sc.write('  <property name="pin17inv" type="bool" value="' + dInvert['17'] + '"/>\n')
sc.write('  <property name="pin1inv" type="bool" value="' + dInvert['10'] + '"/>\n')
sc.write('  <property name="pin2" type="string" value="' + dPins['2'] + '"/>\n')
sc.write('  <property name="pin2inv" type="bool" value="' + dInvert['2'] + '"/>\n')
sc.write('  <property name="pin3" type="string" value="' + dPins['3'] + '"/>\n')
sc.write('  <property name="pin3inv" type="bool" value="' + dInvert['3'] + '"/>\n')
sc.write('  <property name="pin4" type="string" value="' + dPins['4'] + '"/>\n')
sc.write('  <property name="pin4inv" type="bool" value="' + dInvert['4'] + '"/>\n')
sc.write('  <property name="pin5" type="string" value="' + dPins['5'] + '"/>\n')
sc.write('  <property name="pin5inv" type="bool" value="' + dInvert['5'] + '"/>\n')
sc.write('  <property name="pin6" type="string" value="' + dPins['6'] + '"/>\n')
sc.write('  <property name="pin6inv" type="bool" value="' + dInvert['6'] + '"/>\n')
sc.write('  <property name="pin7" type="string" value="' + dPins['7'] + '"/>\n')
sc.write('  <property name="pin7inv" type="bool" value="' + dInvert['7'] + '"/>\n')
sc.write('  <property name="pin8" type="string" value="' + dPins['8'] + '"/>\n')
sc.write('  <property name="pin8inv" type="bool" value="' + dInvert['8'] + '"/>\n')
sc.write('  <property name="pin9" type="string" value="' + dPins['9'] + '"/>\n')
sc.write('  <property name="pin9inv" type="bool" value="' + dInvert['9'] + '"/>\n')
sc.write('  <property name="pp2_direction" type="int" value="0"/>\n')
sc.write('  <property name="pp3_direction" type="int" value="0"/>\n')
sc.write('  <property name="pyvcp" type="bool" value="False"/>\n')
sc.write('  <property name="pyvcpconnect" type="bool" value="False"/>\n')
sc.write('  <property name="pyvcphaltype" type="int" value="0"/>\n')
sc.write('  <property name="pyvcpname" type="string" value="blank.xml"/>\n')
sc.write('  <property name="s32in" type="float" value="10.0"/>\n')
sc.write('  <property name="s32out" type="float" value="10.0"/>\n')
sc.write('  <property name="spindlecarrier" type="float" value="100.0"/>\n')
sc.write('  <property name="spindlecpr" type="float" value="100.0"/>\n')
sc.write('  <property name="spindlefiltergain" type="float" value="0.01"/>\n')
sc.write('  <property name="spindlenearscale" type="float" value="1.5"/>\n')
sc.write('  <property name="spindlepwm1" type="float" value="0.2"/>\n')
sc.write('  <property name="spindlepwm2" type="float" value="0.8"/>\n')
sc.write('  <property name="spindlespeed1" type="float" value="100.0"/>\n')
sc.write('  <property name="spindlespeed2" type="float" value="800.0"/>\n')
sc.write('  <property name="stepspace" type="float" value="5000.0"/>\n')
sc.write('  <property name="steptime" type="float" value="5000.0"/>\n')
sc.write('  <property name="tempexists" type="int" value="0"/>\n')
sc.write('  <property name="units" type="int" value="' + units + '"/>\n')
sc.write('  <property name="usespindleatspeed" type="bool" value="False"/>\n')
sc.write('  <property name="xhomepos" type="float" value="0.0"/>\n')
sc.write('  <property name="xhomesw" type="float" value="0.0"/>\n')
sc.write('  <property name="xhomevel" type="float" value="0.05"/>\n')
sc.write('  <property name="xlatchdir" type="int" value="0"/>\n')
sc.write('  <property name="xleadscrew" type="float" value="%f"/>\n'% xleadscrew)
sc.write('  <property name="xmaxacc" type="float" value="%f"/>\n'% xmaxacc)
sc.write('  <property name="xmaxlim" type="float" value="%f"/>\n'% xmaxlim)
sc.write('  <property name="xmaxvel" type="float" value="1.0"/>\n')
sc.write('  <property name="xmicrostep" type="float" value="1.0"/>\n')
sc.write('  <property name="xminlim" type="float" value="%f"/>\n'% xminlim)
sc.write('  <property name="xpulleyden" type="float" value="1.0"/>\n')
sc.write('  <property name="xpulleynum" type="float" value="1.0"/>\n')
sc.write('  <property name="xscale" type="float" value="0.0"/>\n')
sc.write('  <property name="xsteprev" type="float" value="200.0"/>\n')
sc.write('  <property name="yhomepos" type="float" value="0.0"/>\n')
sc.write('  <property name="yhomesw" type="float" value="0.0"/>\n')
sc.write('  <property name="yhomevel" type="float" value="0.05"/>\n')
sc.write('  <property name="ylatchdir" type="int" value="0"/>\n')
sc.write('  <property name="yleadscrew" type="float" value="%f"/>\n'% xleadscrew)
sc.write('  <property name="ymaxacc" type="float" value="%f"/>\n'% ymaxacc)
sc.write('  <property name="ymaxlim" type="float" value="%f"/>\n'% ymaxlim)
sc.write('  <property name="ymaxvel" type="float" value="1.0"/>\n')
sc.write('  <property name="ymicrostep" type="float" value="1.0"/>\n')
sc.write('  <property name="yminlim" type="float" value="%f"/>\n'% yminlim)
sc.write('  <property name="ypulleyden" type="float" value="1.0"/>\n')
sc.write('  <property name="ypulleynum" type="float" value="1.0"/>\n')
sc.write('  <property name="yscale" type="float" value="8000.0"/>\n')
sc.write('  <property name="ysteprev" type="float" value="200.0"/>\n')
sc.write('  <property name="zhomepos" type="float" value="0.0"/>\n')
sc.write('  <property name="zhomesw" type="float" value="0.0"/>\n')
sc.write('  <property name="zhomevel" type="float" value="0.05"/>\n')
sc.write('  <property name="zlatchdir" type="int" value="0"/>\n')
sc.write('  <property name="zleadscrew" type="float" value="%f"/>\n'% zleadscrew)
sc.write('  <property name="zmaxacc" type="float" value="%f"/>\n'% zmaxacc)
sc.write('  <property name="zmaxlim" type="float" value="%f"/>\n'%zmaxlim)
sc.write('  <property name="zmaxvel" type="float" value="1.0"/>\n')
sc.write('  <property name="zmicrostep" type="float" value="1.0"/>\n')
sc.write('  <property name="zminlim" type="float" value="%f"/>\n'%xminlim)
sc.write('  <property name="zpulleyden" type="float" value="1.0"/>\n')
sc.write('  <property name="zpulleynum" type="float" value="1.0"/>\n')
sc.write('  <property name="zscale" type="float" value="8000.0"/>\n')
sc.write('  <property name="zsteprev" type="float" value="200.0"/>\n')
sc.write("</stepconf>")

# close the file
sc.close()

rm = open("README", "wb")
if len(errors) > 0:
  rm.write("The following errors were found during processing.\n")
  rm.write(errors+"\n")
  print errors
rm.write("This file can be deleted after running the Stepconf Wizard once.\n")
rm.write("Copy the generated .stepconf file to the linuxcnc/configs directory.\n")
rm.write("Run the Stepconf Wizard from the CNC menu.\n")
rm.write("Select Modify a configuration already created with this program.\n")
rm.write("Pick the converted file from the list.\n")
rm.write("If the file did not show up then you have not copied it to the correct directory.\n")
rm.write("Run the latency test on page three with Test Base Period Jitter.\n")
rm.write("Allow the test to run for at least a hour then enter the largest Max Jitter.\n")
rm.write("Go through each screen and check for correctness then save when done.")
rm.close()

print 'Mach import/conversion done'
