#!/usr/bin/env python3
# Copyright 2007 Ben Lipkowitz
# You may distribute this software under the GNU GPL v2 or later
#
# Hexapod visualization.
# Typical Hal connections require both joint and axis values:
#   net skgui.L  genhexkins.gui.L hexagui.axis.L  (L= x,y,z,a,b,c)
#   net jN       joint.N.pos-fb   hexagui.joint.N (N= 0..5)


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
instance = "hexagui"

for setting in sys.argv[1:]:
    exec(setting)

c = WsComponent([
    "joint.0", "joint.1", "joint.2", "joint.3", "joint.4", "joint.5", "axis.x", "axis.y", "axis.z", "axis.a", "axis.b", "axis.c",
])

# Start WS subscription in background thread.
t = threading.Thread(target=_ws_thread, args=(c, instance), daemon=True)
t.start()



#################################
#draw it!

minitetra = 1
#stolen from genhexkins.h 
# you must change these if you are not using minitetra
# positions of base strut ends in base (world) coordinates	
base_offsets = list(range(6))
base_offsets[0] = (-22.950, 13.250, 0)
base_offsets[1] = (22.950, 13.250, 0)
base_offsets[2] = (22.950, 13.250, 0)
base_offsets[3] = (0, -26.5, 0)
base_offsets[4] = (0, -26.5, 0)
base_offsets[5] = (-22.950, 13.250, 0)

# position of platform strut end in platform coordinate system 
plat_offsets = list(range(6))
plat_offsets[0] = (-1, 11.5, 0)
plat_offsets[1] = (1, 11.5, 0)
plat_offsets[2] = (10.459, -4.884, 0)
plat_offsets[3] = (9.459, -6.616, 0)
plat_offsets[4] = (-9.459, -6.616, 0)
plat_offsets[5] = (-10.459, -4.884, 0)



scale = 1
tool_len = 3
plat_radius = 11.5
plat_thickness = 2
base_radius = 28
base_thickness = 3
strut_length = 21
strut_radius = 1
#not used if joint coordinates are defined
angles = [5, 115, 125, 235, 245, 355]

#provide some reference frames
world_coords = Capture()
#i shouldn't have to do this
foo = Collection([world_coords, Sphere(0,0,0,0)])
foo = Translate([foo],0,0,0)

#tool_coords starts out at the origin
tool_coords = Capture()
work_coords = Capture()

blob = CylinderZ(-tool_len, 0.3, 0, 0)
tool = Collection([tool_coords, blob])
#tool = Translate([tool],0,0,-tool_len)
base = CylinderZ(0, base_radius, base_thickness, base_radius)
platform = CylinderZ(0, plat_radius, plat_thickness, plat_radius)

#this can probably be removed
workpiece = Box(0,0,0,4,3,2)
workpiece = Collection([work_coords, workpiece])
workpiece = Translate([workpiece],0,0,base_thickness)

struts = []
base_joints = []
plat_joints = []

for i in range(6):
  #the end-cap is so we can always see where the cylinder is
  inner = CylinderZ(0, 0.8*strut_radius, strut_length, 0.8*strut_radius)
  endcap = CylinderZ(strut_length-1,1.2*strut_radius,strut_length,1.2*strut_radius)
  inner = Collection([inner, endcap])
  outer = CylinderZ(0, 1*strut_radius, strut_length, 1*strut_radius)
  #account for joint offset
  inner = Translate([inner],0,0,-30)
  # make strut extend and retract
  hal_pin = "joint." + str(i)
  inner = HalTranslate([inner],c,hal_pin,0,0,scale)
  inner = Translate([inner],0,0,strut_length)
  strut = Collection([inner, outer])
  
  #build platform  
#  plat_joint_coords += [Capture()]
  plat_joint_coords = Capture()
  plat_joint = BoxCentered(1,1,2)
  plat_joint = Collection([plat_joint, plat_joint_coords])
  #put the joints at weird locations to make an octahedron
  if(minitetra):
    plat_joint = Rotate([plat_joint],-120*(i%2)+60*i, 0,0,1)
    x,y,z = plat_offsets[i]
    plat_joint = Translate([plat_joint],x,y,z) 
  else:
    plat_joint = Translate([plat_joint], 0.8*plat_radius,0,-plat_thickness)
    plat_joint = Rotate([plat_joint], angles[i]-120*(i%2)+60, 0,0,1)
  plat_joints += [plat_joint]
  
  #build base
#  base_joint_coords += [Capture()]
  base_joint_coords = Capture()
  base_joint = BoxCentered(2,3,1.5)
  base_joint = Collection([base_joint, base_joint_coords])
  #put the joints at weird locations to make an octahedron
  if(minitetra):
    base_joint = Rotate([base_joint],-120*(i%2)+60*i, 0,0,1)
    x,y,z = base_offsets[i]
    base_joint = Translate([base_joint],x,y,z)
  else:
    base_joint = Translate([base_joint], 0.8*base_radius,0,0)
    base_joint = Rotate([base_joint], angles[i], 0,0,1)
  base_joints += [base_joint]
  
  #point strut at platform - this also translates the strut to the base joint
  #because i couldn't figure out how to rotate it around the base of the strut
  strut = Track([strut],base_joint_coords, plat_joint_coords, world_coords)
  struts += [strut]


base = Translate([base],0,0,-base_thickness)

#de-listify
struts = Collection(struts[:])
plat_joints = Collection(plat_joints[:])
base_joints = Collection(base_joints[:])

base = Collection([base, base_joints])
platform = Collection([platform,plat_joints])

platform = Translate([platform],0,0,-(plat_thickness+tool_len))
platform = Collection([tool, platform])

####
#animate it
#must rotate first or we will be rotating around the origin
platform = HalRotate([platform], c, "axis.a",1,1,0,0)
platform = HalRotate([platform], c, "axis.b",1,0,1,0)
platform = HalRotate([platform], c, "axis.c",1,0,0,1)
platform = HalTranslate([platform],c, "axis.x",1,0,0)
platform = HalTranslate([platform],c, "axis.y",0,1,0)
platform = HalTranslate([platform],c, "axis.z",0,0,1)
platform = Collection([platform])

#put struts under platform - not perfect, oh well
#this way tool tip stays at origin so no backplot lines at startup
#struts = Translate([struts],0,0,-strut_length)
base = Translate([base],0,0,-strut_length)
workpiece = Translate([workpiece],0,0,-strut_length)

#myhud = Hud()
#myhud.show("welcome!")

#myhud.debug_track = 0
model = Collection([platform, struts, base, workpiece, foo])

#main(model, tool_coords, work_coords, size=30, hud=myhud)
main(model, tool_coords, work_coords, size=30, lat=-65, lon=-45)

