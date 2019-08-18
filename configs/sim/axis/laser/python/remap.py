from pyhal import *
from raster import *
from interpreter import INTERP_EXECUTE_FINISH, INTERP_OK

rasterProgrammer = None

def init(self):
    global rasterProgrammer
    rasterProgrammer = RasterProgrammer("raster-programmer")
    print "raster initialized"
       

def rasterClear(self, **words):
    #machine must be in position before clear issued
    #make motion complete queue prior to clearing raster
    yield INTERP_EXECUTE_FINISH
    global rasterProgrammer
    if rasterProgrammer:
        rasterProgrammer.clear()
        
def rasterBegin(self, **words):
    global rasterProgrammer
    #machine must be in position before begin issued
    #make motion complete queue prior to begin program
    yield INTERP_EXECUTE_FINISH
    if rasterProgrammer:
        count = int(words['p'])
        rasterProgrammer.begin(count)


def rasterData(self, **words):
    global rasterProgrammer
    if rasterProgrammer:
        comment = self.blocks[self.remap_level].comment
        data = comment.strip(',').split(',')
        if len(data) % 2 != 0:
            "raster data must have an even number of arguments. given {0}".format(len(data))
        else:
            for i in range(0, len(data), 2):
                rasterProgrammer.data(float(data[i]), float(data[i+1]))
    return INTERP_OK


def rasterRun(self, **words):
    global rasterProgrammer
    #matchine must be in position before run issued
    #make motion complete queue prior to run program command
    yield INTERP_EXECUTE_FINISH
    if rasterProgrammer:
        rasterProgrammer.run()
    

