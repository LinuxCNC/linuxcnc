#!/usr/bin/python2

import re, sys, os
from pprint import pprint

class hexint(int):
    '''Int class that prints as hex'''
    def __str__(self): return hex(self)
    def __repr__(self): return hex(self)

def pinmux_pin(pins={}):
    '''Get pin control offset'''

    dirname = "/sys/bus/platform/drivers/pinctrl-single/44e10800.pinmux/of_node"
    pin_re = re.compile(r'^pinmux_P([0-9])_([0-9]*)_.*_pin$')
    for n in os.listdir(dirname):
        m = pin_re.match(n)
        if not m:  continue
        header, num = [int(i) for i in m.groups()]
        if (header,num) not in pins:
            pins[(header,num)] = dict(header=header, num=num)
        if 'offset' in pins[(header,num)]: continue
        with open("%s/%s/pinctrl-single,pins" % (dirname,n), 'r') as f:
            b = f.read(8)
            # 0x800 base + 2nd word
            pins[(header,num)]['offset'] = hexint(((8+ord(b[2]))<<8) + (ord(b[3])))
    return pins

def gpio_info(pins={}):
    '''Get pin direction, gpio chip and number'''

    # Get GPIO info
    ocp_dir = "/sys/devices/platform/ocp"
    gpios = {}
    # - Get list of gpio devices
    gpio_dirs = [i for i in os.listdir(ocp_dir) if i.endswith('.gpio') ]
    # - Get port
    gpiochip_re = re.compile(r'^gpiochip([0-9]*)$')
    for d in gpio_dirs:
        dirname = "%s/%s" % (ocp_dir,d)
        chipdirs = [i for i in os.listdir(dirname)
                 if gpiochip_re.match(i) ]
        if len(chipdirs) != 1:
            print "Error:  want exactly one 'gpiochip*' entry in %d" % dirname
            sys.exit(1)
        chipno = int(gpiochip_re.match(chipdirs[0]).group(1))
        gpios[chipno] = dict(
            dirname="%s/gpiochip%s" % (dirname, chipno))
    gpio_re = re.compile(r'^gpio([0-9]*)$')
    p_re = re.compile(r'^P([0-9]*)_([0-9]*)$')
    for chip, info in gpios.items():
        dirname = info['dirname']
        for gpioname in os.listdir('%s/gpio' % dirname):
            gpionum = int(gpio_re.match(gpioname).group(1))

            gpio_dir = '%s/gpio/gpio%d' % (dirname, gpionum)
            with open("%s/label" % gpio_dir, 'r') as f:
                m = p_re.match(f.read())
                header = int(m.group(1))
                pin_num = int(m.group(2))
            with open("%s/direction" % gpio_dir, 'r') as f:
                direction = f.read()[:-1]
            pin = pins.setdefault((header,pin_num),{})
            pin['header'] = header
            pin['direction'] = direction
            pin['chip'] = chip
            pin['gpio'] = gpionum
            pin['chipgpio'] = gpionum - (32*chip)
    return pins

def pinctrl_names(pins={}):
    '''Get pin function names'''

    ocp_dir = "/sys/firmware/devicetree/base/ocp"
    pinmux_re = re.compile(r'^P([0-9])_([0-9]*)_pinmux$')
    for sub_dir in os.listdir(ocp_dir):
        m = pinmux_re.match(sub_dir)
        if not m:  continue
        header, num = [int(i) for i in m.groups()]
        with open("%s/%s/pinctrl-names" % (ocp_dir,sub_dir), 'r') as f:
            pins.setdefault((header, num), {})['funcs'] = f.read().split(chr(0))[:-1]
    return pins


def print_beaglebone_gpio_h(pins, board_name, board_id):
    '''Format pin data suitable for appending to beaglebone_gpio.h'''

    max_pins = 0; hlo = 100; hhi = 0
    for header, pin in pins:
    	if header > hhi: hhi = header
        if header < hlo: hlo = header
        if max_pins < pin: max_pins = pin

    print "#define %s %d  // board ID" % (board_name, board_id)
    print "#define %s_PINS_PER_HEADER %d" % (board_name, max_pins)
    print "#define %s_HLO_HEADER %d" % (board_name, hlo)
    print "#define %s_HHI_HEADER %d" % (board_name, hhi)

    for h,n in ((hlo, "HLO"), (hhi,"HHI")):
        print "pb_gpio_pin %s_%s_PINS[%s_PINS_PER_HEADER+1] = {" % (
            board_name, n, board_name)
        print
        for p in range(max_pins):
            data = pins.get((h,p), dict(chip=-1, num=p, offset=-1, chipgpio=-1))
            print ("\t{ NULL, %(chip)2d, %(chipgpio)2d, %(offset)5s, 0 }, "
                   "// pin %(num)d" % data)
        print "};"
        print

# Command line args
if len(sys.argv) != 3:
   sys.stderr.write("Usage: $s BOARDNAME ID\n")
   sys.exit(1)
board_name = sys.argv[1]
board_id = int(sys.argv[2])

# Scrape sysfs for pin data
pins = {}
pinmux_pin(pins)
gpio_info(pins)
pinctrl_names(pins)

# Print pin data
#pprint(pins)
print_beaglebone_gpio_h(pins, board_name, board_id)
