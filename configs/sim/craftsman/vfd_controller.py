#!/bin/bash/python
########	    VFD Spindle Controller   #########
####	      Creator: Piet van Rensburg 	 #####
####          Company: Craftsman CNC         #####
####          wwww.craftsmancnc.co.nz        #####
##################################################

import sys, os
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
#sys.path.append('/home/craftsmancnc/linuxcnc/configs/craftsmancnc/')
sys.path.append('./')

import serial
import linuxcnc
import globals

import easygui
import traceback
import time

####globals####
SerialByte =  ['0','1', '2','3', '4', '6','5', '7', '8']
############

def exception_format(e):
	info = "".join(traceback.format_tb(sys.exc_info()[2]))
	return str(e) + "\n\n" + info
	
	#used like this
	#try:
	#	......
	#except Exception, e:
	#	easygui.msgbox(exception_format(e), title="Exception")
    
#ttyS0 = COM1
#ttyS1 = COM2  
Comm_Addr = '/dev/ttyS1'
ComPort1 = serial.Serial(Comm_Addr, 9600)
ComPort1.baudrate = 9600
ComPort1.bytesize = serial.EIGHTBITS #number of bits per bytes
ComPort1.parity = serial.PARITY_NONE #set parity check: no parity
ComPort1.stopbits = serial.STOPBITS_ONE #number of stop bits

def start_spindle_ccw():
#def start_spindle_cw():
	try:    
		globals.get_spindledata()		
		if ComPort1.isOpen and globals.spindle_controll == '1':
			ComPort1.write(chr(2) + chr(3) + chr(1) + chr(17) + chr(48) + chr(0))
			globals.ops_message('Spindle started CCW....')
			#globals.ops_message('Spindle started CW....')
	except Exception,  e:
		#easygui.msgbox(exception_format(e), title="VFD Exception")
		pass

    
def start_spindle_cw():
#def start_spindle_ccw():
	try:
		globals.get_spindledata()		
		if ComPort1.isOpen and globals.spindle_controll == '1':
			ComPort1.write(chr(2) + chr(3) + chr(1) + chr(1) + chr(49) + chr(204)) 
			globals.ops_message('Spindle started CW....')
			#globals.ops_message('Spindle started CCW....')    
	except Exception,  e:
		#easygui.msgbox('Unable to start spindle - ccw', title="VFD Error") 
		pass
	
	
def set_spindle_speed(speed):
	try:
		globals.get_spindledata()
		if ComPort1.isOpen and globals.spindle_controll == '1':
			prepare_speed(speed)	
			ComPort1.write(chr(int(SerialByte[0], 16)) + chr(int(SerialByte[1], 16)) + chr(int(SerialByte[2], 16)) + chr(int(SerialByte[3], 16)) + chr(int(SerialByte[4], 16)) +  chr(int(SerialByte[5], 16)) + chr(int(SerialByte[6], 16)) + chr(int(SerialByte[7], 16)))
			time.sleep(0.1)
			globals.ops_message('Spindle speed set at ' + '%.0f' % (float(speed)) + ' RPM')
	except Exception,  e:
		#easygui.msgbox(exception_format(e), title="VFD Exception") #'Unable to set spindle speed', title="VFD Error") 
		pass
	
def stop_spindle():
	try:
		if ComPort1.isOpen:
			ComPort1.write(chr(2) + chr(3) + chr(1) + chr(8) + chr(241) + chr(202))
			try:
				if open('messages', 'rb').read().find('Insert tool' ) == -1:				
					globals.ops_message('Spindle stopped....')
			except: pass
				
	except Exception,  e:
		#easygui.msgbox('Unable to stop spindle', title="VFD Error") 
		pass	
		
def prepare_speed(speed):
	ibuf = [2, 5, 3, 82, 35, 0]
	global SerialByte
	try:		
		SerialByte[0]  = '02'
		SerialByte[1]  = '05'
		SerialByte[2]  = '03'
		
		DecString = "%x" % (((float(speed)  / 24000 ) * 400) *100 ) 
		
		if len(DecString) == 1:
			DecString = '000' + DecString
		if len(DecString) == 2:
			DecString = '00' + DecString
		if len(DecString) == 3:
			DecString = '0' + DecString   	

		SerialByte[3]  = DecString[0] + DecString[1]
		SerialByte[4]  = DecString[2] + DecString[3]	

		SerialByte[5]  = '00'

		calc_crch(SerialByte[0] + SerialByte[1] + SerialByte[2] + SerialByte[3] + SerialByte[4] + SerialByte[5], DecString)
	except Exception,  e:
		#easygui.msgbox(exception_format(e), title="VFD Error") 
		pass
		
def calc_crch(dec_code, decstr):	
	Constant = '1010000000000001'
	Start =    '1111111111111111'
	TempBin =  '0000000000000000'
	Bin = '00000000' + bin(int('02', 16))[:2].zfill(8) 
	TempBin.zfill(16)
	Bin.zfill(16)
	l = 0
	
	for k in range(0,  6):
		Code = dec_code[k+l] + dec_code[k+l+1]
		l = l + 1
		Bin = '00000000' + bin(int(Code, 16))[2:].zfill(8)
		
		#---------------Start-----------------------
		TempBin =  '0000000000000000'
		Last = Start[len(Start)-1]
		for i in range(0,  len(Start)):
			if Bin[i] == Start[i] :
				TempBin = TempBin[:i] + '0' + TempBin[:(len(TempBin) - 1) - i]
			else:
				TempBin = TempBin[:i] + '1' + TempBin[:(len(TempBin) - 1) - i]		
		Bin = TempBin	
		#--------------End start--------------------	
		
		for j in range(0,  8) :
			Last = Bin[len(Bin)-1]
			TempBin =  '0000000000000000'
			if Last == '1' :
				for i in range(0,  len(Constant)) :
					if i == 0 :
						if Constant[i]  == '0' :
							TempBin = TempBin[:i] + '0' + TempBin[:(len(TempBin) - 1) - i]
						else :
							TempBin = TempBin[:i] + '1' + TempBin[:(len(TempBin) - 1) - i]
					else :
						if Bin[i-1] == Constant[i] :
							TempBin = TempBin[:i] + '0' + TempBin[:(len(TempBin) - 1) - i]
						else :
							TempBin = TempBin[:i] + '1' + TempBin[:(len(TempBin) - 1) - i]
			
				Bin = TempBin
			else :
				Bin = Bin[:-1]
				Bin = '0' + Bin
		Start = Bin
	
		CRCH = Bin
		CRCH = CRCH[8:]
	
		CRCL = Bin
		CRCL = CRCL[:8]


	SerialByte[6] = '%.2x' %  int(CRCH, 2)
	SerialByte[7] = '%.2x' %  int(CRCL, 2)

#EOF
