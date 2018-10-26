#!/usr/bin/env python
#    This is a component of linuxcnc
#    mitsub_vfd Copyright 2017 Chris Morley
#
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

# user space component for controlling a misubishi inverter over the serial port using rs485 standard
# specifcally the A500 F500 E500 A500 D700 E700 F700 series - others may work or need adjustment
# tested on A500 E500 and E700
# I referenced manual 'communication option reference manual' and A500 technical manual for 500 series.
# 'Fr-A700 F700 E700 D700 technical manual' for the 700 series
#
# The inverter must be set manually for communication ( you may have to set PR 77 to 1 to unlock PR modifcation )
# must power cycle the inverter for some of these to register eg 79
# PR 79 - 1 or 0
# PR 117 station number - 1               (can be optionally set 0 - 31) if component is also set
# PR 118 communication speed 96           (can be optionally set 48,96,192) if component is also set
# PR 119 stop bit/data length - 0         8 bits, two stop (don't change)
# PR 120 parity - 0                       no parity (don't change)
# PR 121 COM tries - 10                   if 10 (maximuim) COM errors then inverter faults (can change)
# PR 122 COM check time interval 9999     (never check) if communication is lost inverter will not know (can change)
# PR 123 wait time - 9999 -               no wait time is added to the serial data frame (don't change)
# PR 124 CR selection - 0                 don't change

import time,hal
import serial
import traceback
class mitsubishi_serial:

    def __init__(self,vfd_names=[['mitsub_vfd','00']],baudrate=9600,port='/dev/ttyUSB0'):
        try:
            self.ser = serial.Serial(
            port,
            baudrate,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_TWO,
            bytesize=serial.EIGHTBITS
            )
            self.ser.open()
            self.ser.isOpen()
        except:
            print "ERROR : mitsub_vfd - No serial interface found at %s"% port
            raise SystemExit
        print "Mitsubishi VFD serial computer link has loaded"
        print "Port: %s,\nbaudrate: %d\n8 data bits, no parity, 2 stop bits\n"%(port,baudrate)

        self.h=[]
        self.comp_names = vfd_names
        for index,name in enumerate(self.comp_names):
            #print index,' NAME:',name[0],' SLAVE:',name[1]
            c = hal.component(name[0])
            c.newpin("fwd", hal.HAL_BIT, hal.HAL_IN)
            c.newpin("run", hal.HAL_BIT, hal.HAL_IN)
            c.newpin("up-to-speed", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("alarm", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("debug", hal.HAL_BIT, hal.HAL_IN)
            c.newpin("monitor", hal.HAL_BIT, hal.HAL_IN)
            c.newpin("motor-cmd", hal.HAL_FLOAT, hal.HAL_IN)
            c.newpin("motor-fb", hal.HAL_FLOAT, hal.HAL_OUT)
            c.newpin("motor-amps", hal.HAL_FLOAT, hal.HAL_OUT)
            c.newpin("motor-power", hal.HAL_FLOAT, hal.HAL_OUT)
            c.newpin("scale-cmd", hal.HAL_FLOAT, hal.HAL_IN)
            c.newpin("scale-fb", hal.HAL_FLOAT, hal.HAL_IN)
            c.newpin("scale-amps", hal.HAL_FLOAT, hal.HAL_IN)
            c.newpin("scale-power", hal.HAL_FLOAT, hal.HAL_IN)
            c.newpin("estop", hal.HAL_BIT, hal.HAL_IN)
            c.newpin("stat-bit-0", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("stat-bit-1", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("stat-bit-2", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("stat-bit-3", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("stat-bit-4", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("stat-bit-5", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("stat-bit-6", hal.HAL_BIT, hal.HAL_OUT)
            c.newpin("stat-bit-7", hal.HAL_BIT, hal.HAL_OUT)
            # set reasonable defaults
            c['scale-cmd'] = 1
            c['scale-fb'] = 1
            c['scale-amps'] = 1
            c['scale-power'] = 1
            c['fwd'] = 1
            # flags for each device
            self['last_run%d'%index] = c['run']
            self['last_fwd%d'%index] = c['fwd']
            self['last_cmd%d'%index] = c['motor-cmd']
            self['last_monitor%d'%index] = c['monitor']
            self['last_estop%d'%index] = c['estop']
            #add device to component reference variable
            self.h.append(c)
            print "Mitsubishi %s VFD: slave# %s added\n"%(name[0],name[1])
        # only issue ready when all the components are ready
        for i in self.h:
            i.ready()

    def loop(self):
        cmd = data = out = temp=''
        while 1:
            try:
              for index,ids in enumerate(self.comp_names):
                self.slave_num = ids[1]
                # MONITOR for up-to-speed, alarms and running frequency
                # 7A is the address for 8 status bits ( b0 - b8 )
                # These bits are configurable from the panel. We assume bit 3 is up to speed
                # and bit 7 is alarm
                # the returned data is 2 characters of hex
                # we convert that to binary and the mask for the bits we want
                if self.h[index]['monitor']:
                    while self.ser.inWaiting() > 0:
                        raw = self.ser.read(1)
                    word = self.prepare_data("7A",None)
                    out = temp = ''
                    self.ser.write(word)
                    time.sleep(.05)
                    string,chr_list,chr_hex = self.poll_output()
                    #print 'DEBUG: ',chr_list,chr_hex
                    if chr_list != '':
                        #print string
                        try:
                            binary = "{0:#010b}".format(int(string[3:5],16))
                        except:
                            binary = '0b000000'
                        self.h[index]['stat-bit-0'] = int(binary,2) & 1
                        self.h[index]['stat-bit-1'] = int(binary,2) & 2
                        self.h[index]['stat-bit-2'] = int(binary,2) & 4
                        self.h[index]['stat-bit-3'] = self.h[index]['up-to-speed'] = int(binary,2) & 8
                        self.h[index]['stat-bit-4'] = int(binary,2) & 16
                        self.h[index]['stat-bit-5'] = int(binary,2) & 32
                        self.h[index]['stat-bit-6'] = int(binary,2) & 64
                        self.h[index]['stat-bit-7'] = self.h[index]['alarm'] = int(binary,2) & 128
                        if self.h[index]['debug'] and 1==2:
                            print 'monitor operation:',binary,temp,temp[3:5],len(temp)

                    # 6F is the address for running motor frequency status
                    # it returns 4 characters of hex
                    # we convert to decimal and multiply by .01 for hertz and by user scale-fb
                    # for arbrtrary units. This does require scale to be set to something besides 0!
                    # we assume the inverter is set to show running hertz (it's configurable in the VFD)
                    word = self.prepare_data("6F",None)
                    out = temp = ''
                    self.ser.write(word)
                    time.sleep(.05)
                    string,chr_list,chr_hex = self.poll_output()
                    if self.h[index]['debug']:
                        print 'DEBUG: ',chr_list,chr_hex
                    if chr_list != '':
                        decimal = int(string[3:7],16)
                        self.h[index]["motor-fb"] = decimal *.01 * self.h[index]["scale-fb"]
                        if self.h[index]['debug'] and 1==2:
                            print 'monitor frequency:',decimal,string,string[3:7], len(string)

                # amps
                    word = self.prepare_data("70",None)
                    out = temp = ''
                    self.ser.write(word)
                    time.sleep(.05)
                    time.sleep(.05)
                    string,chr_list,chr_hex = self.poll_output()
                    if self.h[index]['debug']:
                        print 'DEBUG: ',chr_list,chr_hex
                    if chr_list != '':
                        decimal = int(string[3:7],16)
                        self.h[index]["motor-amps"] = decimal *.01 * self.h[index]["scale-amps"]
                        if self.h[index]['debug'] and 1==2:
                            print 'monitor amps:',decimal,string,string[3:7], len(string)


                # STOP ON ESTOP
                # if ESTOP is false it stops the output
                # when ESTOP is reset the run command must be re-issued (cycled false to true) to start motor 
                if not self['last_estop%d'%index] == self.h[index]['estop']:
                    if not self.h[index]["estop"]:
                        cmd = "FA";data ="00"
                        word = self.prepare_data(cmd,data)
                        self.ser.write(word)
                        time.sleep(.05)
                        self['last_estop%d'%index] = self.h[index]['estop']
                        print "**** Mitsubishi VFD: %s stopped due to Estop Signal"% ids[0]
                        continue
                    print "**** Mitsubishi VFD: Estop cleared - Must re-issue run command to start %s." % ids[0]
                    self['last_estop%d'%index] = self.h[index]['estop']

                # SET RUN AND DIRECTION
                # address FA sets the start and direction
                # it expects a 2 character hex representing a 8 bit (b0 - b7) binary number
                # bit 1 sets forward, 4 sets reverse, 0 stop
                # depending on the inverter and options other bits are possible,
                # but these three are consistant
                if not self['last_run%d'%index] == self.h[index]['run'] or not self['last_fwd%d'%index] == self.h[index]['fwd']:
                    if self.h[index]['run']:
                        if self.h[index]['fwd']:
                            cmd = "FA";data ="02"
                        else:
                            cmd = "FA";data ="04"
                    else:
                        cmd = "FA";data ="00"
                    word = self.prepare_data(cmd,data)
                    self.ser.write(word)
                    time.sleep(.05)
                    self['last_run%d'%index] = self.h[index]['run']
                    self['last_fwd%d'%index] = self.h[index]['fwd']
                    if self.h[index]['debug']:
                        string,chr_list,chr_hex = self.poll_output()
                        print 'DEBUG: ',chr_list,chr_hex

                # SET cmd
                # address ED is for setting the running frequency
                # it expects 4 characters of hex representing frequency in .01 hertz units
                # we internally scale it by 100 to make it 1 hertz units and by user scale
                # for arbrtrary units. This does require scale to be set to something besides 0!
                if not self['last_cmd%d'%index] == self.h[index]['motor-cmd']:
                    freq = int(abs(self.h[index]['motor-cmd']*100*self.h[index]['scale-cmd']))
                    if freq > 40000: freq = 40000
                    if freq < 0: freq = 0
                    self['last_cmd%d'%index] = self.h[index]['motor-cmd']
                    # send frequency command
                    cmd="ED";data="%0.4X"%freq
                    word = self.prepare_data(cmd,data)
                    self.ser.write(word)
                    time.sleep(.05)
                    if self.h[index]['debug']:
                        string,chr_list,chr_hex = self.poll_output()
                        print 'DEBUG: ',chr_list,chr_hex

            except KeyboardInterrupt:
                    self.kill_output()
                    raise
            except:
                    print "error",ids
                    print sys.exc_info()[0]

    def kill_output(self):
        cmd = "FA";data ="00"
        for index,ids in enumerate(self.comp_names):
            self.slave_num = ids[1]
            word = self.prepare_data(cmd,data)
            self.ser.write(word)
            time.sleep(.05)
            print 'Mitsub VFD: Kill-> ', ids[0]

    def prepare_data(self,command ='E1',data= '07AD'):
        combined = self.slave_num+command  +'1'
        if not data == None:
            combined += data
        s=0
        for i in range(0,len(combined)):
            letter = combined[i]
            s=s+ord(letter.upper())
        converted_data = chr(0x5) + combined + hex(s)[-2:-1].upper() + hex(s)[-1:].upper()
        return converted_data

    def poll_output(self):
        string = chr_list_out = hex_out = ''
        while self.ser.inWaiting() > 0:
            raw = self.ser.read(1)
            chr_list_out += raw+','
            string += raw
            hex_out += hex(ord(raw))
            hex_out +=' '
        if hex_out != '':
            answ=''
            if chr_list_out[0] == chr(0x6):
                answ = 'ackg'
            if chr_list_out[0] == chr(0x15):
                answ = 'error'
            if chr_list_out[0] == chr(0x2):
                answ = 'checksome error'
            #print 'slave:',chr_list_out[2]+chr_list_out[4],answ
            return string,chr_list_out,hex_out
        return '','',''

    def __getitem__(self, item):
        return getattr(self, item)
    def __setitem__(self, item, value):
        return setattr(self, item, value)

if __name__ == "__main__":
    import getopt,sys
    letters = 'p:b:h' # the : means an argument needs to be passed after the letter
    keywords = ['port=', 'baud=' ] # the = means that a value is expected after 
    # the keyword
    opts, extraparam = getopt.getopt(sys.argv[1:],letters,keywords) 
    # starts at the second element of argv since the first one is the script name
    # extraparms are extra arguments passed after all option/keywords are assigned
    # opts is a list containing the pair "option"/"value"
    port='/dev/ttyS0'
    device_ids=[]
    baud=9600

    for o,p in opts:
      if o in ['-p','--port']:
         port = p
      elif o in ['-b','--baud']:
         baud = p
      elif o in ['-h','--help']:
        print 'Mitsubishi VFD computer-link interface'
        print ' User space component for controlling a misubishi inverter over the serial port using the rs485 standard'
        print ' specifcally the A500 F500 E500 A500 D700 E700 F700 series - others may work or need small adjustments'
        print ''' I referenced manual 'communication option reference manual' and A500 technical manual for 500 series.'''
        print ''' 'Fr-A700 F700 E700 D700 technical manual' for the 700 series'''
        print 
        print ' The inverter must be set manually for communication ( you may have to set PR 77 to 1 to unlock PR modifcation )'
        print ' You must power cycle the inverter for some of these to register eg 79'
        print ' PR 79 - 1 or 0                          sets the inverter to respond to the PU/computer-link'
        print ' PR 117 station number (slave) - 1       can be optionally set 0 - 31 if component is also set'
        print ' PR 118 communication speed 96           baud rate, can be optionally set 48,96,192 if component is also set'
        print ''' PR 119 stop bit/data length - 1         8 bits, two stop (don't change)'''
        print ''' PR 120 parity - 0                       no parity (don't change)'''
        print ' PR 121 COM tries - 10                   if 10 (maximuim) COM errors then inverter faults (can change)'
        print ' PR 122 COM check time interval 9999     (never check) if communication is lost inverter will not know (can change)'
        print ''' PR 123 wait time - 9999 -               no wait time is added to the serial data frame (don't change)'''
        print ''' PR 124 CR selection - 0                 don't change'''
        print '''
This driver assumes certain other VFD settings:
-That the  motor frequency status is set to show herts.
-That the status bit 3 is up to speed
-That the status bit 7 is alarm
'''
        print
        print '''some models (eg E500) cannot monitor status -set the monitor pin to false
in this case pins such as up-to-speed, amps, alarm and status bits are not useful.
'''
        print '''HAL command used to load: '''
        print '''loadusr mitsub_vfd --baud 4800 --port /dev/ttyUSB0 NAME=SLAVE_NUMBER 
        -NAME is user selectable (usually a description of controlled device)
        -SLAVE_NUMBER is the slave number that was set on the VFD
        -NAME=SLAVE_NUMBER can be repeated for multiple VFD's connected together
        --baud is optional as it defaults to 9600
        all networked vfds must be set to the same baudrate
        --port is optional as it defaults to ttyS0'''
        print
        print ''' Sample linuxcnc code
loadusr -Wn coolant mitsub_vfd spindle=02 coolant=01
# **************** Spindle VFD setup slave 2 *********************
net spindle-vel-cmd               spindle.motor-cmd
net spindle-cw                    spindle.fwd              
net spindle-on                    spindle.run              
net spindle-at-speed              spindle.up-to-speed
net estop-out                     spindle.estop
#       cmd scaled to RPM
setp spindle.scale-cmd .135
#       feedback is in rpm
setp spindle.scale-fb 7.411
setp spindle.monitor 1

net spindle-speed-indicator spindle.motor-fb        gladevcp.spindle-speed
# *************** Coolant vfd setup slave 3 ***********************
net coolant-flood                         coolant.run
net coolant-is-on              coolant.up-to-speed   gladevcp.coolant-on-led
#       cmd and feedback scaled to hertz
setp coolant.scale-cmd 1
setp coolant.scale-fb 1
#       command full speed
setp coolant.motor-cmd 60
#       allows us to see status
setp coolant.monitor 1
net estop-out                     coolant.estop
'''
        sys.exit(0)
    if extraparam:
        for dids in extraparam:
            device_ids.append(dids.split('='))
    else:
        device_ids=[['mitsub_vfd','00']]
    print port,baud
    # Info gathered now use them
    try:
        app = mitsubishi_serial(vfd_names=device_ids,baudrate=int(baud),port=port)
        app.loop()
    except KeyboardInterrupt:
        sys.exit(0)
    else:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            formatted_lines = traceback.format_exc().splitlines()
            print
            print "**** Mitsub_vfd debugging:",formatted_lines[0]
            traceback.print_tb(exc_traceback, limit=1, file=sys.stdout)
            print formatted_lines[-1]
