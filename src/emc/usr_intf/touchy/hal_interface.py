# Touchy is Copyright (c) 2009  Chris Radek <chris@timeguy.com>
#
# Touchy is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Touchy is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.



import hal

class hal_interface:
    def __init__(self):
        self.c = hal.component("touchy")
        self.c.newpin("jog.x", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.y", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.z", hal.HAL_BIT, hal.HAL_OUT)
        self.c.newpin("jog.increment", hal.HAL_FLOAT, hal.HAL_OUT)
        self.c.ready()
        self.jogaxis(0)

    def jogaxis(self, n):
        self.c["jog.x"] = n == 0
        self.c["jog.y"] = n == 1
        self.c["jog.z"] = n == 2

    def jogincrement(self, inc):
        incs = [0.01, 0.001, 0.0001]
        self.c["jog.increment"] = incs[inc]
        
