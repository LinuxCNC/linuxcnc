#!/usr/bin/env python3
#**************************************************************************
#
# Copyright 2023 David Mueller mueller_david@hotmail.com
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#**************************************************************************

#--------------------------------------------------------------------------
# Visualization model of a 5-axis milling machine
# with table rotary axes A and B
# This model includes dynamic adjustments of geometric offsets of
# rotational axes as well as the position of the intersection of the
# rotational axes A and B (rotation-point of the rotary-assembly).
#--------------------------------------------------------------------------


from vismach import *
import math
import sys
import json
import threading
import asyncio
import websockets
from gmi import ws_url


class WsComponent:
    """Dict-like object fed by WebSocket subscription, compatible with vismach."""

    def __init__(self, pins):
        self._values = {name: 0.0 for name in pins}

    def __getitem__(self, key):
        return self._values.get(key, 0.0)

    def __setitem__(self, key, value):
        self._values[key] = float(value)

    def __getattr__(self, key):
        if key.startswith('_'):
            raise AttributeError(key)
        return self._values.get(key.replace('_', '-'), 0.0)

    def update(self, data):
        """Update pin values from a WS message (full or delta)."""
        if isinstance(data, dict):
            for k, v in data.items():
                if k in self._values:
                    self._values[k] = float(v)


def _ws_thread(comp, instance):
    """Background thread: subscribe to haljson WS and push updates to comp."""
    async def _run():
        url = ws_url()
        sub_msg = json.dumps({
            "action": "subscribe",
            "api": instance,
            "instance": instance,
            "func": "pins",
            "rate_ms": 50,
        })
        while True:
            try:
                async with websockets.connect(url) as ws:
                    await ws.send(sub_msg)
                    async for msg in ws:
                        data = json.loads(msg)
                        if data.get("type") == "update":
                            comp.update(data.get("data", {}))
            except Exception:
                await asyncio.sleep(1)

    asyncio.run(_run())


# Instance name — matches the haljson instance loaded in the HAL file.
instance = "xyzab-tdr-gui"

for setting in sys.argv[1:]:
    exec(setting)

c = WsComponent([
    "axis-x", "axis-y", "axis-z", "rotary-a", "rotary-b", "x_offset", "z_offset", "x-rot-point", "y-rot-point", "z-rot-point", "tool_length", "tool_diameter",
])

# Start WS subscription in background thread.
t = threading.Thread(target=_ws_thread, args=(c, instance), daemon=True)
t.start()


# These values are arbitrary and are meant to simulate an absolute
# machine zero that is offset from the rotation point of the rotary assembly.
# Machine zero as measured from rotation point of the rotary assembly
machine_zero_x =  50
machine_zero_y =  50
machine_zero_z =  100

for setting in sys.argv[1:]: exec(setting)

# give endpoint Z values and radii
# resulting cylinder is on the Z axis
class HalToolCylinder(CylinderZ):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        r = 2 # default if hal pin not set
        if (c.tool_diameter > 0): r = c.tool_diameter/2
        return -c.tool_length, r, 0, r

# used to create visual indicators for the x-offset
class HalOffsetCylinderX(CylinderX):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        length = 0 # default if hal pin not set
        length = c.x_offset
        return length, 0.1, 0, 0.3

# used to create visual indicators for the z-offset
class HalOffsetCylinderZ(CylinderZ):
    def __init__(self, comp, *args):
        CylinderZ.__init__(self, *args)
        self.comp = c

    def coords(self):
        length = 0 # default if hal pin not set
        length = c.z_offset
        return length, 0.1, 0, 0.3


# begin tool-side
tooltip = Capture()

# create the tool using the values for tool_diameter and tool_length in the tool table
tool = Collection([HalTranslate([tooltip], c, "tool_length", 0,0,-1),
                   HalToolCylinder(c),
                   ])
tool = Color([1,0,0,0], [tool] )

# create a visual indicator for the position of the "(x,y,z)-rot-point"
# so the user can see the effects of the "(x,y,z)_rot-point" value changes
rot_point_set = Collection([
                Color([1,0.8,0,1],[CylinderX(-5,0.2,5,0.2)]),
                Color([1,0.8,0,1],[CylinderY(-5,0.2,5,0.2)]),
                Color([1,0.8,0,1],[CylinderZ(-5,0.2,5,0.2)])
                ])
# Now move the rot_point indicator from the home position by the
# '(x,y,z)_rot-point' values
# For the kinematics to work correctly the 'rot_point_set' indicator must be
# shown in the same position as the red sphere indicating the rotation point
# of the rotary assembly created later
rot_point_set = HalTranslate([rot_point_set],c,"x-rot-point", 1,0,0)
rot_point_set = HalTranslate([rot_point_set],c,"y-rot-point", 0,1,0)
rot_point_set = HalTranslate([rot_point_set],c,"z-rot-point", 0,0,1)

spindle = Collection([
          # spindle nose and/or toolholder
          CylinderZ( 0, 10, 20, 15),
          # spindle housing
          CylinderZ( 20, 20, 100, 20),
          ])

head_y = Collection([
         tool,
         Color([0,0.5,0.5,0], [spindle]),
         # y-slide, holds spindle
         Color([0,1,0,0], [Box( -30, -30, 60, 30, 280, 135 )])
         ])
# add the hal connections for x,y,z axis movements
head_y = HalTranslate([head_y],c,"axis-x",1,0,0)
head_y = HalTranslate([head_y],c,"axis-y",0,1,0)
head_y = HalTranslate([head_y],c,"axis-z",0,0,1)
# move the spindle and y-slide to the simulated machine home position
head_y     = Translate([head_y], machine_zero_x, machine_zero_y, machine_zero_z)
# also move the rot_point indicator along with the spindle
# it's 'location from the spindle differs by (x,y,z)-rot-point values
rot_point_set  = Translate([rot_point_set], machine_zero_x, machine_zero_y, machine_zero_z)
# end tool-side

# begin work-side
work = Capture()

# create rotary A
table_a = Collection([
         work,
         CylinderX(-18, 50, 0, 50),
         # cross
         Color([1,1,1,0], [BoxCentered(0.1, 100, 2)]),
         Color([1,1,1,0], [BoxCentered(0.1, 2, 100)]),
         # lump on one side
         Color([1,1,1,0], [Box(-10, -2, -50, 2, 2, -40)])
         ])
# add the hal connection for the a axis rotation
table_a = HalRotate([table_a],c,"rotary-a",1,-1,0,0)
table_a = Color([1,0,1,0], [table_a] )

rotary_a = Collection([
          table_a,
          # rotary table base - block under the rotary A table
          Color([1,0.5,0,0], [Box(-50,-50, -50, -10, 100, 50)])
          ])

# simulate the 'x-offset' and 'z-offset' values
rotary_a = HalTranslate([rotary_a],c,"x_offset",1,0,0)
rotary_a = HalTranslate([rotary_a],c,"z_offset",0,0,1)

rotary_b = Collection([
       # arm
       Color([1,0.5,0,0], [CylinderY(80,50,100,50)]),
       Color([1,0.5,0,0], [Box(-70,80,-50,0,100,50)]),
       # white 'dowel' indicating the axis B rotational center
       Color([1,1,1,0], [CylinderY(75,0.2,80,2)]),
       Color([1,1,1,0], [CylinderY(0,0.1,80,0.2)]),
       #  ball at the end for the rotation-point of the rotary-assembly
       Color([1,0.1,0,0],[ Sphere(0,0,0,0.5)]),
       # show geometric offsets of the rotary-assembly
       # z-offset extending from the rotation-point
       Color([0,0,1,1],[HalOffsetCylinderZ(CylinderZ)]),
       # x-offset extending from the end of the z-offset indicator (should end at the face center of the rotary A)
       HalTranslate([Color([1,0,0,1],[HalOffsetCylinderX(CylinderX)])], c, "z_offset", 0,0,1),
       ])

dualrotary = Collection([
             rotary_a,
             rotary_b,
             ])
# add the hal connections for b axis rotation
dualrotary = HalRotate([dualrotary],c,"rotary-b",1,0,-1,0)
# end work-side

# begin base
base = Collection([
       # base
       Box(-120, -100, -250, 120, 160, -150),
       # column
       Box(-120, 100, -250, 120, 200, 300),
       ])
base = Color([0.5,0.5,0.5,0], [base] )
# end base

# begin model
model = Collection([
        head_y,
        dualrotary, base,
        rot_point_set
        ])
# end model

# begin hud
myhud = Hud()
myhud.show("xyzab-tdr-gui")
# end hud

main(model, tooltip, work, size=500, hud=myhud, lat=-60, lon=0)
