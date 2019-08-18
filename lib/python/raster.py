"""
    The raster module is useful for controlling the raster realtime component.
"""
from pyhal import *
from struct import *
from time import sleep, clock

class ProgrammerException(Exception):
    """exception type raised by the raster programmer"""
    pass

faultCode_OK = 0
"""the fault code representing no error from raster"""


class RasterProgrammer(object):
    """
        The programmer component is used to program the realtime raster component

        The raster component operates on line at a time. 

        Example:
            Assuming rastering is done accross the X axis.
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
        self.component = component(name)
        self.port = self.component.pinNew("program", halType.PORT, pinDir.OUT)
        self.faultCode = self.component.pinNew("fault-code", halType.UNSIGNED, pinDir.IN)
        self.enabled = self.component.pinNew("enabled", halType.BIT, pinDir.IN)
        self.component.ready()
        self.timeout = 0.01 #wait for 1/100 of a second for before buffer becomes writable

    def __del__(self):
        self.component.exit()

    def exit(self):
        """unloads this programmer component from hal"""
        self.component.exit()
       
    def __writeCmd(self, cmd):
        t = clock()
        while((clock() - t) < self.timeout):
            if self.port.writable() > len(cmd):
                self.port.write(cmd)
                return
            else:
                sleep(0)

        raise ProgrammerException("Programmer exceeded timeout waiting for port to become writable")
            
    def __checkStatus(self):
        if self.faultCode.value != faultCode_OK:
            raise ProgrammerException("Raster faulted with code {0}".format(self.faultCode.value))

    def __waitEnabled(self, enabled):
        while self.faultCode.value == faultCode_OK and enabled != self.enabled.value:
            sleep(0)

    def clear(self):
        """
            Sends the program clear command to the raster. 
            Waits until the raster program is cleared.
        """
        self.__checkStatus()
        self.__writeCmd("clr;")
        self.__waitEnabled(False)
        
    def begin(self, count):
        """
            Sends the program begin command along with the number of data points
            that the raster should expect
        """
        self.__checkStatus()
        self.__writeCmd("beg{0};".format(count))

    def data(self, pos, power):
        """
            Sends a data point to the raster programmer. Each subsequent data point position
            must be greater than the previous or the raster will fault
        """
        self.__checkStatus()
        self.__writeCmd("dat{0:.3f},{1:.3f};".format(pos, power))

    def run(self):
        """
            Commands the raster to begin running the program
            Waits until the raster is enabled and running
        """
        self.__checkStatus()
        self.__writeCmd("run;")
        self.__waitEnabled(True)
