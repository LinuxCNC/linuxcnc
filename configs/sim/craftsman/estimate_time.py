#!/bin/bash/python
# estimate_time.py

########	    Estimate machine time       #########
####	      Creator: Piet van Rensburg    	  #####
####          Company: Craftsman CNC          #####
####          wwww.craftsmancnc.co.nz         #####
###################################################
import math
import ConfigParser_New

import globals

##arc length
#d = math.sqrt(math.pow(x1 - x2, 2) + math.pow(y1 - y2,  2) + math.pow(z1 - z2, 2))
#a = math.pow(math.sin(2),  -1) * (d/(r * 2))
#l = r * a  

##vector length
#d = math.sqrt(math.pow(x1 - x2, 2) + math.pow(y1 - y2,  2) + math.pow(z1 - z2, 2))


x1 = x2 = y1 = y2 = z1 = z2 = a1 = a2 = f = r = 0

x_jog = y_jog = z_jog = a_jog = 0

mach_time = 0
calculated_time = '00:00:00'

def insert(original, new, pos):
	return original[:pos] + new + original[pos:]

def read_gcodefile(file):
	global mach_time
	mach_time = 0
	
	f = open('time.log','w')
	f.close()
	
	global x_jog 
	global y_jog
	global z_jog
	global a_jog
	
	config = ConfigParser_New.RawConfigParser() 
	config.read('craftsmancnc.ini')
	x_jog = config.get('AXIS_0', 'MAX_VELOCITY') #,  '80')
	y_jog = config.get('AXIS_1', 'MAX_VELOCITY') #,  '80') #80
	z_jog = config.get('AXIS_2', 'MAX_VELOCITY') #30
	a_jog = config.get('AXIS_3', 'MAX_VELOCITY') #850
	
	f = open(file,'r')
	for data in f.readlines():	
		if data.find('G') > -1:
			data = data[:-1]
			gcode_decode(data)
	f.close()


def gcode_decode(gcode):
	global x1
	global x2
	global y1
	global y2
	global z1
	global z2
	global a1
	global a2
	global f
	global r
	global x_jog 
	global y_jog
	global z_jog
	global a_jog
	
	has_x = False
	has_y = False
	has_z = False		
	has_a = False
	has_i = False
	has_j = False
	has_k = False		
	has_r = False
	
	feedrates = []
		
	#capitalise gcode
	gcode.upper()	
		
	#insert space infront of code if no space
	if gcode.find('G') > -1 or gcode.find('F') > -1:
		if gcode.find('G') - 1 != ' ' and gcode.find('G') > 0 :
			gcode = insert(gcode, ' ', gcode.find('G'))
		if gcode.find('X') > 0 : 
			if gcode[gcode.find('X') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('X'))
		if gcode.find('Y') > 0 : 
			if gcode[gcode.find('Y') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('Y'))
		if gcode.find('Z') > 0 : 
			if gcode[gcode.find('Z') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('Z'))
		if gcode.find('A') > 0 : 
			if gcode[gcode.find('A') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('A'))
		if gcode.find('F') > 0 : 
			if gcode[gcode.find('F') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('F'))
		if gcode.find('I') > 0 : 
			if gcode[gcode.find('I') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('I'))
		if gcode.find('J') > 0 : 
			if gcode[gcode.find('J') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('J'))
		if gcode.find('K') > 0 : 
			if gcode[gcode.find('K') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('K'))
		if gcode.find('R') > 0 : 
			if gcode[gcode.find('R') - 1] != ' ':
				gcode = insert(gcode, ' ', gcode.find('R'))
	
		
	#read codes
	gcode = gcode.split(' ')
	for c in range(0, len(gcode)):
		#G
		if gcode[c].find('G') > -1:			
			gc = int(gcode[c][1:])	
		#X
		if gcode[c].find('X') > -1:
			has_x = True
			feedrates.extend([float(x_jog)])
			x2 = float(gcode[c][1:])			
		#Y
		if gcode[c].find('Y') > -1:
			has_y = True
			feedrates.extend([float(y_jog)])
			y2 = float(gcode[c][1:])			
		#Z
		if gcode[c].find('Z') > -1:
			has_z = True
			feedrates.extend([float(z_jog)])
			z2 = float(gcode[c][1:])			
		#A
		if gcode[c].find('A') > -1:
			has_a = True
			feedrates.extend([float(a_jog)])
			a2 = float(gcode[c][1:])			
		#F
		if gcode[c].find('F') > -1:
			f = float(gcode[c][1:])
		#I
		if gcode[c].find('I') > -1:
			has_i = True
			i = float(gcode[c][1:])
		#J
		if gcode[c].find('J') > -1:
			has_j = True
			j = float(gcode[c][1:])
		#K
		if gcode[c].find('K') > -1:
			has_k = True
			k = float(gcode[c][1:])
		#R
		if gcode[c].find('R') > -1:
			has_r = True
			r = float(gcode[c][1:])	
		
		
	try:
		d = math.sqrt(math.pow(x1 - x2, 2) + math.pow(y1 - y2,  2) + math.pow(z1 - z2, 2))
	except: pass
		
			
	if has_i or has_j or has_k and not has_r:
		try:
#		if i != 0 and j != 0 : 
			r = math.sqrt(math.pow(x1 - (x2 + i), 2) + math.pow(y2 - (y2 + j),  2))
#		if k != 0 :r = j
			a = math.pow(math.sin(2),  -1) * (d/(r * 2))
			d = (r * a)*2  
		except: pass

	if has_r:
		a = math.pow(math.sin(2),  -1) * (d/(r * 2))
		d = (r * a) * 2  	

	x1 = x2
	y1 = y2
	z1 = z2
	a1 = a2
	
	has_x = False
	has_y = False
	has_z = False		
	has_a = False
	has_i = False
	has_j = False
	has_k = False		
	has_r = False
		
	i = j = k = r = 0	
	
	if gc == 0: 
		feed = float(min(feedrates)) * 60
	else:
		feed = f
	
	calculate_move_time(d, feed)
	
def calculate_move_time(dist, feed):
	global mach_time
	global calculated_time
	
	if feed == 0:
		feed = 1
		
	#calculate accelaration
	ft = 0
	acc = 900
	at = float(acc) / feed 
	
	ad = (acc * math.pow(at,  2)) * 0.5
	
	fsd = dist - (ad * 2)	
	
	if fsd > 0 :
		ft = float(fsd)/feed
		
	if dist >= ad + ad: 
		t = ft + (ad * 2)
	else:
		t = at * (float(dist) / (ad * 2)) 
	
	t = t * 60
	
	mach_time = mach_time + t
	
	seconds = mach_time 
	minutes = seconds // 60
	hours = minutes // 60
	
	
	#write_f("%.3f" % t + ' ' + "%.3f" % mach_time + ' ' + "%02d:%02d:%02d" % (hours, minutes % 60, seconds % 60) + ' ' + "%.0f" % feed + ' ' + "%.3f" % dist  + ' 5b')
	calculated_time = ("%02d:%02d:%02d" % (hours, minutes % 60, seconds % 60))

def write_f(var):
	f = open('time.log','ab')
	f.write(var + '\n')
	f.close()
