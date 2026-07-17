#!/usr/bin/env python3
import hal
import time
import os
from raster import *

theta = 0.005
timeout = 0.25

def fleq(a, b):
    return abs(a - b) < theta

def assertIdle(pin):
    assert not pin['fault'].value, "Fault detected"
    assert pin['state'].value == 0, "raster not idle"

def waitEnabled(pin):
    stop = time.process_time() + timeout
    while not pin['enabled'].value:
        time.sleep(0.001);
        assert not pin['fault'].value, "Fault occurred. Code {0}".format(pin['fault_code'].value)
        assert time.process_time() < stop, "Timeout while waiting"

def waitDisabled(pin):
    stop = time.process_time() + timeout
    while pin['enabled'].value:
        time.sleep(0.001);
        assert not pin['fault'].value, "Fault occurred. Code {0}".format(pin['fault_code'].value)
        assert time.process_time() < stop, "Timeout while waiting"

def waitFault(pin):
    stop = time.process_time() + timeout
    while not pin['fault'].value:
        time.sleep(0.001);
        assert time.process_time() < stop, "Timeout while waiting"

def waitIdle(pin):
    stop = time.process_time() + timeout
    while pin['state'].value != 0:
        time.sleep(0.001);
        assert not pin['fault'].value, "Fault occurred. Code {0}".format(pin['fault_code'].value)
        assert time.process_time() < stop, "Timeout while waiting"

def resetRaster(prog, pin):
    stop = time.process_time() + timeout
    prog.run.value = 0
    pin['reset'].value = 1
    while pin['state'].value != 0:
        time.sleep(0.001);
        assert time.process_time() < stop, "Timeout while waiting"
    pin['reset'].value = 0
    assertIdle(pin)


def testValidBegin(prog, pin):
    resetRaster(prog, pin)
    prog.port.write("123.45;4;1.0;10;0000000000")
    prog.run.value = 1

    waitEnabled(pin)

    assert not pin['fault'].value, "Fault detected."
    assert fleq(pin['offset'].value, 123.45), "Offset was not set correctly"
    assert pin['bpp'].value == 4, "BPP was not set correctly, Given:{0}".format(pin['bpp'].value)
    assert fleq(pin['ppu'].value, 1.0), "PPU was not set correctly"
    assert pin['count'].value == 10, "count was not set correctly"

    prog.run.value = 0
    waitIdle(pin)
    assertIdle(pin)


def testInvalidOffset(prog, pin):
    resetRaster(prog, pin)
    prog.port.write("FOOB;41.0;2;00")
    prog.run.value = 1

    waitFault(pin)
    assert pin['fault'].value, "Fault not detected"
    assert pin['fault_code'].value == FaultCodes.InvalidOffset.value


def testInvalidBPP(prog, pin):
    for bpp in [0, 2, 10, 14, 54, -50.0]:
        resetRaster(prog, pin)
        prog.port.write("123.45;{0};1.0;2;00".format(bpp))
        prog.run.value = 1
        waitFault(pin)
        assert pin['fault_code'].value == FaultCodes.InvalidBPP.value

def testValidBPP(prog, pin):
    for bpp in [4, 8, 12, 16, 20, 24, 28, 32]:
        resetRaster(prog, pin)
        prog.port.write("123.45;{0};1.0;2;{1}".format(bpp, "0" * 2 * int(bpp/4)))
        prog.run.value = 1
        waitEnabled(pin)

def testInvalidPPU(prog, pin):
    for ppu in [-1.0, 0.0]:
        resetRaster(prog, pin)
        prog.port.write("123.45;4;{0};2;00".format(ppu))
        prog.run.value = 1
        waitFault(pin)
        assert pin['fault_code'].value == FaultCodes.InvalidPPU.value

def testValidPPU(prog, pin):
    for tppu in [0.01, 1423.2343, 1000]:
        resetRaster(prog, pin)
        prog.port.write("123;4;{0};2;00".format(tppu))
        prog.run.value = 1
        waitEnabled(pin)
        assert fleq(pin['ppu'].value, tppu), "PPU failed to be set"

def testValidCount(prog, pin):
    for tcount in [2, 10, 500]:
        resetRaster(prog, pin)
        prog.port.write("-156;4;1.0;{0};{1}".format(tcount, "0" * tcount))
        prog.run.value = 1
        waitEnabled(pin)
        assert pin['count'].value == tcount, "Count was not valid"

def testInvalidCount(prog, pin):
    for tcount in [0, 1, -1, -100]:
        resetRaster(prog, pin)
        prog.port.write("2334.1223;4;1.0;{0};00000000".format(tcount))
        prog.run.value = 1
        waitFault(pin)
        assert pin['fault_code'].value == FaultCodes.InvalidCount.value, \
            "Not expected fault code {0}. tcount {1}".format(pin['fault_code'].value, tcount)

def testWrongDirection(prog, pin):
    resetRaster(prog, pin)
    pin['position'].value = -10
    prog.port.write("10;4;1.0;10;0123456789")
    prog.run.value = 1
    waitEnabled(pin)

    pin['position'].value = -11
    waitFault(pin)
    assert pin['fault_code'].value == FaultCodes.WrongDirection.value, \
            "Expected fault WrongDirection. Got {0}".format(pin['fault_code'].value)

    resetRaster(prog, pin)
    pin['position'].value = 10
    prog.port.write("-10;4;1.0;10;0123456789")
    prog.run.value = 1
    waitEnabled(pin)
    pin['position'].value = 11
    waitFault(pin)
    assert pin['fault_code'].value == FaultCodes.WrongDirection.value, \
            "Expected fault WrongDirection. Got {0}".format(pin['fault_code'].value)


def testProgram(prog, pin, pos, offset, bpp, dpu, program, data):
    pin['position'].value = pos
    prog.begin(offset, bpp, dpu, int(len(program)/(bpp/4)))
    prog.data(program)

    prog.start()
    for (pos, pwr) in data:
        pin['position'].value = pos
        time.sleep(0.001)
        assert (pin['output'].value - pwr) < theta, \
            "output at position {0} should be {1}. Got {2}".format(pos, pwr, pin['output'].value)
        #once position is exceeded the program should be finished and raster should reset
    assert pin['enabled'].value == True, "raster should still be enabled"
    prog.stop()


def main():
    try:
        c = hal.component("test")

        pin = {
            'reset': c.newpin("reset", hal.HAL_BIT, hal.HAL_OUT),
            'enabled': c.newpin("enabled", hal.HAL_BIT, hal.HAL_IN),
            'position': c.newpin("position", hal.HAL_FLOAT, hal.HAL_OUT),
            'output': c.newpin("output", hal.HAL_FLOAT, hal.HAL_IN),
            'offset': c.newpin("offset", hal.HAL_FLOAT, hal.HAL_IN),
            'bpp': c.newpin("bpp", hal.HAL_S32, hal.HAL_IN),
            'ppu': c.newpin("ppu", hal.HAL_FLOAT, hal.HAL_IN),
            'count': c.newpin("count", hal.HAL_S32, hal.HAL_IN),
            'fault': c.newpin("fault", hal.HAL_BIT, hal.HAL_IN),
            'fault_code': c.newpin("fault-code", hal.HAL_S32, hal.HAL_IN),
            'state': c.newpin("state", hal.HAL_S32, hal.HAL_IN),
            'bitmap_position': c.newpin("bitmap-position", hal.HAL_FLOAT, hal.HAL_IN),
            'current_pixel_index': c.newpin("current-pixel-index", hal.HAL_S32, hal.HAL_IN),
            'current_pixel_value': c.newpin("current-pixel-value", hal.HAL_FLOAT, hal.HAL_IN),
            'previous_pixel_value': c.newpin("previous-pixel-value", hal.HAL_FLOAT, hal.HAL_IN),
            'fraction': c.newpin("fraction", hal.HAL_FLOAT, hal.HAL_IN),
        }
        c.ready()

        prog = RasterProgrammer("programmer")

        #instantiate the raster component,
        #add it to a thread and link all signals from
        #the test component here
        assert os.system('halcmd -f raster.hal') == 0, "raster.hal script failed"

        testInvalidOffset(prog, pin)
        testInvalidBPP(prog, pin)
        testValidBPP(prog, pin)
        testInvalidPPU(prog, pin)
        testValidPPU(prog, pin)
        testValidCount(prog, pin)
        testInvalidCount(prog, pin)
        #testWrongDirection()
        resetRaster(prog, pin)
        testProgram(prog, pin, -1.0,
                    1.0,
                    4,
                    1.0,
                    "0E",
                    [(-0.5, -1.0),
                     (0.0, 0.0),
                     (0.25, 25.0),
                     (0.5, 50.0),
                     (0.75, 75.0),
                     (1.0, 100.0),
                     (1.1, -1.0)])

        testProgram(prog, pin, 2.0,
                    -1.0,
                    4,
                    1.0,
                    "0E",
                    [(1.1, -1.0),
                     (1.0, 0.0),
                     (0.75, 25.0),
                     (0.5, 50.0),
                     (0.25, 75.0),
                     (0.0, 100.0),
                     (-0.5, -1.0)])

        testProgram(prog, pin, -1.0,
                    1.0,
                    12,
                    1.0,
                    "000FFE",
                    [(-0.5, -1.0),
                     (0.0, 0.0),
                     (0.25, 25.0),
                     (0.5, 50.0),
                     (0.75, 75.0),
                     (1.0, 100.0),
                     (1.1, -1.0)])
    except Exception as e:
        import traceback
        print("error: Test failed: {}".format(traceback.format_exc()))
        return 1
    finally:
        c.exit()
        prog.exit()
    return 0

if __name__ == "__main__":
    exit(main())
