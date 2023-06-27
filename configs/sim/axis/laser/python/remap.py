from pyhal import *
from raster import *
from interpreter import INTERP_EXECUTE_FINISH, INTERP_OK

rasterProgrammer = None

def init(self):
    global rasterProgrammer
    rasterProgrammer = RasterProgrammer("raster-programmer")
    print("raster initialized")
       
        
def rasterBegin(self, **words):
    global rasterProgrammer
    #machine must be in position before begin issued
    #make motion complete queue prior to begin program
    yield INTERP_EXECUTE_FINISH
    if rasterProgrammer:
        offset = float(words['p'])
        bpp = int(words['q'])
        ppu = float(words['r'])
        count = int(words['s'])
        rasterProgrammer.begin(offset, bpp, ppu, count)


def rasterData(self, **words):
    global rasterProgrammer
    if rasterProgrammer:
        comment = self.blocks[self.remap_level].comment.strip()
        rasterProgrammer.data(comment)
    return INTERP_OK


def rasterStart(self, **words):
    global rasterProgrammer
    #machine must be in position before run issued
    #make motion complete queue prior to run program command
    yield INTERP_EXECUTE_FINISH
    if rasterProgrammer:
        rasterProgrammer.start()

def rasterStop(self, **words):
    global rasterProgrammer
    yield INTERP_EXECUTE_FINISH
    if rasterProgrammer:
        rasterProgrammer.stop()
    

