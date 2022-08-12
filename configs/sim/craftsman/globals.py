#!/bin/bash/python
# globals.py

########	       Global variables         #########
####	      Creator: Piet van Rensbur 	  	  #####
####          Company: Craftsman CNC          #####
####          wwww.craftsmancnc.co.nz         #####
###################################################

import linuxcnc
import gtk
import os

def log(mess):
	#try:
	#	f = open('operations.log','ab')
	#	if f : f.write(mess + '\n')
	#	f.close()
	#except: pass
	pass

def clear_log():
	f = open('operations.log', 'w')
	f.close()


mess = ''
def ops_message(mess):
	try:
		f = open('messages','w')
		if f : f.write(mess)
		f.close()
	except: pass
	
def get_message():
	global mess
	f = open('messages', 'r')
	if f : mess = f.readline()
	f.close()
	return mess
	
def tool_message(mess):
	try:
		f = open('tool.dat','w')
		if f : f.write(mess)
		f.close()
	except: pass

def vfd_message(speed):
	try:
		f = open('vfd_speed','w')
		if f : f.write(speed)
		f.close()
	except: pass

change_tool = '0' 
def tool_change(n):
	ops_message('Tool change in progress.....')
	set_toolchangedata(n)
	global change_tool
	change_tool = n
	
def tool_changed():
	set_toolchangedata('0')
	global change_tool
	change_tool = '0'
	
def get_toolchanged():
	global change_tool
	return change_tool
	
def get_toolchangedata():
	global change_tool
	try:
		f2 = open('toolchange.dat',  'r')
		if f2 : change_tool = f2.readline()
		f2.close()
	except: pass
	
def set_toolchangedata(change_tool):
	try:
		f2 = open('toolchange.dat',  'w')
		if f2 : f2.write(change_tool)
		f2.close()
	except: pass
	
def set_tooldata(number):
	data = []
	a = 0
	f = open('tool.tbl','r')
	length = ''
	diameter = ''
	txt = ''
	x = 0
	for data in f.readlines():			
			for i in range(0,  13):				
				if data[i]:					
					if data.find( 'T' + str(number) + ' ') > -1:
						if data.find('Z') > -1: 
							txt = data[data.find('Z'): ] 
							x = txt.find(' ')
							length = data[data.find('Z') +1 : data.find('Z') + x] 		
						if data.find('D') > -1: 
							txt = data[data.find('D'): ] 
							x = txt.find(' ')
							diameter = data[data.find('D') +1: data.find('D') + x]
						if data.find(';') > -1: 
							txt = data[data.find(';'): ] 
							x = txt.find('\n')
							desc = data[data.find(';') +1: data.find(';') + x]
						f2 = open('tool.dat',  'w')
						if f2 : f2.write(str(number) + ',' + diameter + ',' + length + ',' + desc)
						f2.close()
	f.close()

tool = '0'
diameter = '0'
length = '0'
tool_desc = ''
def get_tooldata():
	global tool
	global diameter
	global length
	global tool_desc
	data = []
	f2 = open('tool.dat',  'r')
	data = f2.readline()
	data = data.split(',')
	tool = data[0] 
	diameter = data[1]
	length = data[2]
	tool_desc = data[3]
	f2.close()
	
def clear_tooldata():
	f2 = open('tool.dat',  'w')
	f2.write('0,0,0,No tool')
	f2.close()
	
def clear_extents():
	f2 = open('extents.dat',  'w')
	f2.write('0,0,0,0,0,0')
	f2.close()
	
spindle_controll = '1'
def get_spindledata():
	global spindle_controll
	try:
		f2 = open('spindle_control.dat',  'r')
		if f2 : spindle_controll = f2.readline()
		f2.close()
	except: pass
	
def set_spindledata(spindle_controller):
	try:
		f2 = open('spindle_control.dat',  'w')
		if f2 : f2.write(spindle_controller)
		f2.close()
	except: pass
	
def running(s, do_poll=True):
    if do_poll: s.poll()
    return s.task_mode == linuxcnc.MODE_AUTO and s.interp_state != linuxcnc.INTERP_IDLE

def ensure_mode(s, c, *modes):
    s.poll()
    if not modes: return False
    if s.task_mode in modes: return True
    if running(s, do_poll=False): return False
    c.mode(modes[0])
    c.wait_complete()
    return True

def add_history(file_name):
	tmp = open('temp.dat',  'wb')		
	f = open('history.dat',  'r')		
	tmp.write(file_name + '\n')
	x = 1
	prev_data = ''
	for data in f.readlines():
		if data != (file_name + '\n'):
			if data != prev_data:
				if x < 9:
					tmp.write(data)
					x =+ 1
		prev_data = data
	tmp.close()
	f.close()
	
	os.remove('history.dat')
	os.rename('temp.dat',  'history.dat')

def get_clicked_string(text_view):  
	buffer = text_view.get_buffer() 
	it = buffer.get_iter_at_mark(buffer.get_insert())
	k = it.get_line()
	it = buffer.get_iter_at_line(k)
	i1 = buffer.get_end_iter()
	text = buffer.get_text(it, i1, False)
	text = text.split('\n')
	return text[0]
	
def get_clicked_line(text_view):  
	buffer = text_view.get_buffer() 
	it = buffer.get_iter_at_mark(buffer.get_insert())
	line = it.get_line()
	return line

	
