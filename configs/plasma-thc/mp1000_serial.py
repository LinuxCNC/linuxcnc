#!/usr/bin/python
import hal, time, serial
from threading import Thread

#DEBUG
DEBUG = 0

# Create a new HAL pin which can be hooked to pyvcp
h = hal.component("serial_interface")
h.newpin("vcpt_voltage", hal.HAL_FLOAT, hal.HAL_IN)
h.newpin("vcpc_voltage", hal.HAL_FLOAT, hal.HAL_OUT)
h.ready()

# Serial Stuff, change the serial port address by changing /dev/ttyS0
ser = serial.Serial('/dev/ttyS0', 9600, bytesize=8, parity='N', stopbits=1, timeout=0, xonxoff=0, rtscts=0)

# Initialize
vcp_voltage = 0
thc_voltage = 0
old_vcp_voltage = 0
old_thc_voltage = 0
vcp_voltage_changed = 0
thc_voltage_changed = 0

try:
    #Main Loop
    while 1:
        time.sleep(0.2)

	# Get the VCP voltage from UI
    	vcp_voltage = int(h.vcpt_voltage)

    	# Get voltage reading from THC if it's available, clean up padding, newline,whitespace,etc
    	raw_thc_voltage = ser.readline()
    	if raw_thc_voltage.strip() != '':
    		refined_thc_voltage = raw_thc_voltage.strip()
    		thc_voltage = int(refined_thc_voltage.strip('*'))

	# Update VCP voltage meter with most recent voltage measurement
	h.vcpc_voltage = thc_voltage
		
	# Check if VCP voltage has changed
    	if vcp_voltage != old_vcp_voltage:
		vcp_voltage_changed = 1
    		old_vcp_voltage = vcp_voltage

	# Check if THC voltage has changed, avoid feedback from VCP
    	if thc_voltage != old_thc_voltage and vcp_voltage_changed == 0:
		thc_voltage_changed = 1
    		old_thc_voltage = thc_voltage

	# If voltage numbers don't match anymore we update the right one or reset signals if everything adds up
    	if thc_voltage == vcp_voltage:
		vcp_voltage_changed = 0
		thc_voltage_changed = 0
	else:
		if vcp_voltage_changed == 1 or thc_voltage_changed != 1:
			# Don't send new instructions to the THC if there are things waiting in the buffer
			if ser.inWaiting() == 0:
				ser.write('**' + str(vcp_voltage) + '\n')
				if DEBUG == 1: 
					print "Changing THC Voltage"
			else:
				if DEBUG == 1:
					print "Chars in Serial Reiceive Buffer: " + str(ser.inWaiting())
		else:
			#Not implemented, this could interface with a HALUI component to set the value of the spinbutton in the plasma sidebar
			if DEBUG == 1: 
				print "Changing VCP Voltage"

except KeyboardInterrupt:
    raise SystemExit
