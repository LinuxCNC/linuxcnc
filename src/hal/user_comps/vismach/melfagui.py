#!/usr/bin/env python3

#--------------------------------------------------------------------------
# Visualization model of the Mitsubishi RV6-SDL 6axis serial manipulator
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
instance = "melfagui"

for setting in sys.argv[1:]:
    exec(setting)

c = WsComponent([
    "joint1", "joint2", "joint3", "joint4", "joint5", "joint6", "lnkdx", "lnkdz", "grip",
])

# Start WS subscription in background thread.
t = threading.Thread(target=_ws_thread, args=(c, instance), daemon=True)
t.start()

###################

# show CNC-tooltip position (ie tool trace in window)
tooltip = Capture()

# create finger (tool)
finger1 = CylinderZ(-100, 10, 0, 0.2)

# create visual tool coordinates axes
xaxis = Color([1,0,0,1],[CylinderX(0,3,100,3)])
yaxis = Color([0,1,0,1],[CylinderY(0,3,100,3)])
zaxis = Color([0,0,1,1],[CylinderZ(0,3,100,3)])

# combine tool and coordinate axis = tool-assembly
finger1 = Collection([finger1,tooltip,xaxis,yaxis,zaxis])
finger1 = Rotate([finger1],90,0,1,0)
finger1 = Translate([finger1],150,0,0)
finger1 = Rotate([finger1],180,1,0,0)

try: # Expect files in working directory
    # create toolholder from file
    link7 = AsciiSTL(filename="link7.stl")
    # create wrist from file
    link6 = AsciiSTL(filename="link6.stl")
    # create forearm from file
    link5 = AsciiSTL(filename="link5.stl")
    # create ellbow trom file
    link4 = AsciiSTL(filename="link4.stl")
    # create upper arm from file
    link3 = AsciiSTL(filename="link3.stl")
    # create shoulder from file
    link2 = AsciiSTL(filename="link2.stl")
    # create base (waist) from file
    link1 = AsciiSTL(filename="link1.stl")
except Exception as detail:
    print(detail)
    raise SystemExit("melfagui requires files link[1-7].stl in working directory")
    

# translate/rotate so the joint rotational axis to wrist is in origin
link7 = Rotate([link7],90,0,1,0)
# combine tool-assembly to  toolholder = hand-assembly
link7 = Collection([finger1,link7])
link7 = Color([0.75,0.75,0.75,1],[link7])
# create HAL-link for Joint6 "Hand rotation"
link7 = HalRotate([link7],c,"joint6",1,1,0,0)

link6 = Color([0.9,0.9,0.9,1],[link6])
# rotate model into the correct orientation
link6 = Rotate([link6],90,0,1,0)
# combine hand-assembly to wrist = wrist-assembly
link6 = Collection([link7, link6])
# translate wrist-assembly so joint5 rotation is in origin
link6 = Translate([link6],85,0,0)
# create HAL-link for Joint5 "Wrist rotation"
link6 = HalRotate([link6],c,"joint5",1,0,1,0)

link5 = Color([0.9,0.9,0.9,1],[link5])
# rotate model into the correct orientation
link5 = Rotate([link5],90,1,0,0)
link5 = Rotate([link5],90,0,1,0)
link5 = Rotate([link5],90,0,0,1)
# connect wrist-assembly to forearm = forearm-assembly
link5 = Collection([link6, link5])
# translate forearm-assembly so joint4 rotation in origin
link5 = Translate([link5],425,0,0)
# create HAL-link for Joint4 "Forearm rotation"
link5 = HalRotate([link5],c,"joint4",1,1,0,0)

link4 = Color([0.9,0.9,0.9,1],[link4])
# rotate model into the correct orientation
link4 = Rotate([link4],90,0,1,0)
# combine the forearm-assembly to the ellbow = ellbow-assembly
link4 = Collection([link5, link4])
# translate ellbow-assembly so joint3 rotation is in the origin
link4 = Translate([link4],0,0,100)
# create HAL-link for Joint3 "Ellbow rotation"
link4 = HalRotate([link4],c,"joint3",1,0,1,0)

link3 = Color([0.9,0.9,0.9,1],[link3])
# rotate model into the correct orientation
link3 = Rotate([link3],90,0,1,0)
# connect the ellbow-assembly to the upper arm = upper-arm-assembly
link3 = Collection([link4, link3])
# translate upper-arm-assembly so joint2 rotation is in theorigin
link3 = Translate([link3],0,0,380)
link3 = Rotate([link3],90,0,1,0)
# create HAL-link for Joint2 "Shoulder rotation"
link3 = HalRotate([link3],c,"joint2",1,0,1,0)

link2 = Color([0.9,0.9,0.9,1],[link2])
# combine the upper-arm-assembly to the shoulder = shoulder-assembly
link2 = Collection([link3, link2])
# translate the shoulder-assembly so joint1 rotaton is in the origin
link2 = Translate([link2], 85,0,111)
# create HAL-link for Joint1 "Waist rotation"
link2 = HalRotate([link2],c,"joint1",1,0,0,1)

link1 = Color([0.9,0.9,0.9,1],[link1])
# combine the shoulder-assembly to the base = robot
robot = Collection([link2, link1])
#translate robot base to origin
robot = Translate([robot],0,0,239)


# create visual world coordinates
xaxis0 = Color([1,0,0,1],[CylinderX(0,5,900,5)])
yaxis0 = Color([0,1,0,1],[CylinderY(0,5,900,5)])
zaxis0 = Color([0,0,1,1],[CylinderZ(0,5,900,5)])
coordw = Collection([xaxis0,yaxis0,zaxis0])


# add a floor
floor = Box(-1500,-1500,-1,1500,1500,0.0)
floor = Color([0.5,0.5,0.5,1],[floor])
floor = Collection([floor, xaxis0, yaxis0])


# show
work = Capture()
model = Collection([robot,coordw,work])

main(model, tooltip, work, size=1200, lat=-60, lon=-65)
