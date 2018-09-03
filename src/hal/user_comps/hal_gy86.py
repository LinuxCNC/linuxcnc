#!/usr/bin/python2


# read GY-86 10DOF board via i2c, mirror values to HAL pins
# 3 axis gyroscope + 3 accelerometer + 3 axis magnetic field+air pressure+temperature
# eg http://smart-prototyping.com/GY-86-10DOF-MS5611-HMC5883L-MPU6050-module-MWC-flight-control-sensor-module.html

# from: https://www.snip2code.com/Snippet/85741/Beaglebone-GY-86-driver

# git clone https://github.com/adafruit/adafruit-beaglebone-io-python.git
# cd adafruit-beaglebone-io-python/
# sudo make install
# sudo apt install python-smbus

from Adafruit_I2C import Adafruit_I2C
import math
import time
import argparse
import hal

parser = argparse.ArgumentParser(description='HAL component to read LSM303 Accelerometer values')
parser.add_argument('-n', '--name', help='HAL component name', default="gy86")
parser.add_argument('-b', '--bus_id', help='I2C bus id', default=2)
parser.add_argument('-i', '--interval', help='I2C update interval', default=0.05)
parser.add_argument('-d', '--delay', help='Delay before the i2c should be updated', default=0.0)
args = parser.parse_args()
delayInterval = float(args.delay)

h = hal.component(args.name)
tempPin = h.newpin("temperature", hal.HAL_FLOAT, hal.HAL_OUT)
pressPin = h.newpin("pressure",   hal.HAL_FLOAT, hal.HAL_OUT)

anglexPin = h.newpin("angle-x", hal.HAL_FLOAT, hal.HAL_OUT)
angleyPin = h.newpin("angle-y", hal.HAL_FLOAT, hal.HAL_OUT)
anglezPin = h.newpin("angle-z", hal.HAL_FLOAT, hal.HAL_OUT)

magxPin = h.newpin("mag-x", hal.HAL_FLOAT, hal.HAL_OUT)
magyPin = h.newpin("mag-y", hal.HAL_FLOAT, hal.HAL_OUT)
magzPin = h.newpin("mag-z", hal.HAL_FLOAT, hal.HAL_OUT)

accelxPin = h.newpin("accel-x", hal.HAL_FLOAT, hal.HAL_OUT)
accelyPin = h.newpin("accel-y", hal.HAL_FLOAT, hal.HAL_OUT)
accelzPin = h.newpin("accel-z", hal.HAL_FLOAT, hal.HAL_OUT)

gyroxPin = h.newpin("gyro-x", hal.HAL_FLOAT, hal.HAL_OUT)
gyroyPin = h.newpin("gyro-y", hal.HAL_FLOAT, hal.HAL_OUT)
gyrozPin = h.newpin("gyro-z", hal.HAL_FLOAT, hal.HAL_OUT)

h.ready()

i2c_bus = int(args.bus_id) #  /dev/i2c-2
mpu6050 = Adafruit_I2C(0x68, i2c_bus) # ADC 16bit
hmc5883 = Adafruit_I2C(0x1e, i2c_bus) # ADC 12bit
ms5611  = Adafruit_I2C(0x77, i2c_bus) # ADC 24bit

# mpu6050 i2c master enable bit
mpu6050.write8(0x6A, 0)
# mpu6050 i2c bypass enable bit
mpu6050.write8(0x37, 2)
# mpu6050 turn off sleep mode
mpu6050.write8(0x6B, 0)

# hmc5883 continuous mode
hmc5883.write8(0x02,0x00)


# ms5611 reset
ms5611.writeList(0x1e,[])
time.sleep(2/1000.0)

# ms5611 prom
t=ms5611.readList(0xa2,2)
c1=t[0]*256+t[1]
t=ms5611.readList(0xa4,2)
c2=t[0]*256+t[1]
t=ms5611.readList(0xa6,2)
c3=t[0]*256+t[1]
t=ms5611.readList(0xa8,2)
c4=t[0]*256+t[1]
t=ms5611.readList(0xaa,2)
c5=t[0]*256+t[1]
t=ms5611.readList(0xac,2)
c6=t[0]*256+t[1]
#c1=40127
#c2=36924
#c3=23317
#c4=23282
#c5=33464
#c6=28312



try:
    time.sleep(delayInterval)
    while True:
        #read mpu6050 accelerations in m/s^2
        ax = (mpu6050.readS8(0x3b)*256+mpu6050.readU8(0x3c))/16384.0
        ay = (mpu6050.readS8(0x3d)*256+mpu6050.readU8(0x3e))/16384.0
        az = (mpu6050.readS8(0x3f)*256+mpu6050.readU8(0x40))/16384.0
        #read mpu6050 temperature in C
        tt = (mpu6050.readS8(0x41)*256+mpu6050.readU8(0x42))/340.0 + 36.53
        #read mpu6050 gyroscope in degree/s
        gx = (mpu6050.readS8(0x43)*256+mpu6050.readU8(0x44))/131.0
        gy = (mpu6050.readS8(0x45)*256+mpu6050.readU8(0x46))/131.0
        gz = (mpu6050.readS8(0x47)*256+mpu6050.readU8(0x48))/131.0

        #wait hmc5883 to be ready
        while not hmc5883.readU8(0x09)&0x01 == 1: pass
        #read hmc5883 magnetometer in gauss
        magx = (hmc5883.readS8(0x3)*256+hmc5883.readU8(0x4))/1090.0
        magz = (hmc5883.readS8(0x5)*256+hmc5883.readU8(0x6))/1090.0
        magy = (hmc5883.readS8(0x7)*256+hmc5883.readU8(0x8))/1090.0

        z=math.degrees(math.atan2(magy,magx));
        y=math.degrees(-math.atan2(ax,az));
        x=math.degrees(math.atan2(ay,az));

        ms5611.writeList(0x48,[])
        time.sleep(9/1000.0)
	adc=ms5611.readList(0x00,3)
	d1=adc[0]*65536+adc[1]*256+adc[2]
	#d1=9085466

	ms5611.writeList(0x58,[])
	time.sleep(9/1000.0)
	adc=ms5611.readList(0x00,3)
	d2=adc[0]*65536+adc[1]*256+adc[2]
	#d2=8569150

	dt = d2 - c5*(1<<8)
	temp=2000+ dt*c6/(1<<23)
	off= c2*(1<<16) + c4*dt/(1<<7)
	sens=c1*(1<<15) + c3*dt/(1<<8)
	p=(d1*sens/(1<<21) - off)/(1<<15)

	#print c1,c2,c3,c4,c5,c6,d1,d2,dt,temp,off,sens,p

	tempPin.value = temp/100.0
	pressPin.value = p/100.0

        anglexPin.value = x
        angleyPin.value = y
        anglezPin.value = z

        magxPin.value = magx
        magyPin.value = magy
        magzPin.value = magz

        accelxPin.value = ax
        accelyPin.value = ay
        accelzPin.value = az

        gyroxPin.value = gx
        gyroyPin.value = gy
        gyrozPin.value = gz

except:
    print(("exiting HAL component " + args.name))
    h.exit()
