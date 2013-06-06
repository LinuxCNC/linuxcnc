

all: /lib/firmware/enable-pru-00A0.dtbo load
	
/lib/firmware/enable-pru-00A0.dtbo:  enable-pru-00A0.dts
	../dtc/dtc -O dtb -o /lib/firmware/enable-pru-00A0.dtbo -b 0 -@ enable-pru-00A0.dts

load:
	echo enable-pru > /sys/devices/bone_capemgr.9/slots



