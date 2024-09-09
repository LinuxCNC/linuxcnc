"""
    The raster module is useful for controlling the raster realtime component.
"""
from re import match
from enum import Enum
from pyhal import *
from struct import *
import time

class ProgrammerException(Exception):
    """exception type raised by the raster programmer"""
    pass

class FaultCodes(Enum):
    OK = 0
    InvalidOffset = 1
    InvalidBPP = 2
    InvalidPPU = 3
    InvalidCount = 4
    BadPixelData = 5
    ProgramWrongSize = 6
    WrongDirection = 7

"""the fault code representing no error from raster"""


class RasterProgrammer(object):
    """
        The programmer component is used to program the realtime raster component

        The raster component operates one line at a time. 

        Example:
            Assuming rastering is done across the X axis.
            A simple raster program would be from X=0.0 Power=0.0 to X=1.0 Power=100.0
            As the x axis passes from x0.0 to x1.0 the power will rise from 0 to 100 linearly.

            Because positional data at the hal level is joint position the program must be relative
            to the current position that the machine is at when programming the line.

            Because a port can become full each command waits until the port is available for writing. The commands will 
            raise a ProgrammerException if too much time passes before the write completes. This is usually an indication
            that your PORT buffer isn't large enough
    """

    def __init__(self, name):
        """
            Initializes a user space component with name.

            This component exports pins:
                program - PORT OUT - link to the raster's program port pin. Make sure to size the PORT signal prior to running.
                fault-code - UNSIGNED IN - link to the raster's fault-code pin. Raster programmer uses this to detect errors
                enabled - BIT IN - link to the raster's enabled pin. Raster programmer uses this to wait on rasters activation
        """
        self.__timeout = 5.0
        self.component = component(name)
        self.port = self.component.pinNew("program", halType.PORT, pinDir.OUT)
        self.faultCode = self.component.pinNew("fault-code", halType.SIGNED, pinDir.IN)
        self.enabled = self.component.pinNew("enabled", halType.BIT, pinDir.IN)
        self.run    = self.component.pinNew("run", halType.BIT, pinDir.OUT)
        self.component.ready()

        self.run.value = False

    def __del__(self):
        self.component.exit()

    def exit(self):
        """unloads this programmer component from hal"""
        self.component.exit()
            
    def __waitEnabled(self, enabled):
        timeout = time.process_time() + self.__timeout
        while self.enabled.value != enabled:
            if time.process_time() > timeout:
                self.run.value = False
                raise ProgrammerException("Raster failed to respond before the timeout was reached")

            if self.faultCode.value != FaultCodes.OK.value:
                self.run.value = False
                raise ProgrammerException("Raster faulted with error {0}".format(FaultCodes(self.faultCode.value)))
        
    def begin(self, offset, bpp, ppu, count):
        """
            Sends the program begin command along with relevant parameters.
            
            offset - The relative starting position that the incoming data starts. Data is always programmed from most negative on the axis to most positive.
            bpp - Bits per pixel in increments of 4 bits, up to 32 bits
            ppu - pixels per unit. The number of pixels per machine unit. count * dpu gives the total span of the raster line
            count - the number of pixels that will be programmed on the line     
        """
        self.run.value = False
        self.__waitEnabled(False)

        allowedbpp = set([4, 8, 12, 16, 20, 24, 28, 32])
        if not isinstance(bpp, int):
            raise ProgrammerException("bpp must be an integer")

        if not bpp in allowedbpp:
            raise ProgrammerException("bpp must be one of {0}. Given: {1}".format(allowedbpp, bpp))

        if ppu <= 0.0:
            raise ProgrammerException("dpu must be greater than 0. Given: {0}".format(dpu))

        if not isinstance(count, int):
            raise ProgrammerException("count must be an integer")

        if count <= 0.0:
            raise ProgrammerException("count must be greater than 0. Given: {0}".format(count))

        self.bpp = bpp
        self.ppu = ppu
        self.countBytes = count * (bpp / 4)
        if not self.port.write("{0:f};{1};{2:.5f};{3};".format(offset, bpp, ppu, count)):
            raise ProgrammerException("raster port appears to be full. Try enlarging the port buffer size")


    def data(self, data):
        """
            Sends raster data to the raster component. data is a string of hexadecimal characters
        """

        self.__waitEnabled(False)

        pixelLen = self.bpp / 4

        if (len(data) % pixelLen) != 0:
            raise ProgrammerException("Improper data format.")

        if not match("[a-fA-F0-9]+", data):
            raise ProgrammerException("Data must be in hexadecimal format A-F,a-f,0-9")

        if not self.port.write(data):
            raise ProgrammerException("raster port appears to be full. Try enlarging the port buffer size")

    def start(self):
        """
            Commands the raster to begin running the program
            Waits until the raster is enabled and running
        """
        if self.enabled.value:
            self.enabled.value = False
            raise ProgrammerException("Raster cannot start. It is already running.")

        self.run.value = True
        self.__waitEnabled(True)

    def stop(self):
        """
           Commands the raster to stop running the program
           Waits until the raster is disabled
        """
        self.run.value = False
        self.__waitEnabled(False)
