import sys

def dist_lseg(l1, l2, p):
    "Compute the 3D distance from the line segment l1..l2 to the point p."
    x0, y0, z0 = l1
    xa, ya, za = l2
    xi, yi, zi = p

    dx = xa-x0
    dy = ya-y0
    dz = za-z0 
    d2 = dx*dx + dy*dy + dz*dz
    
    if d2 == 0: return 0

    t = (dx * (xi-x0) + dy * (yi-y0) + dz * (zi-z0)) / d2
    if t < 0: t = 0
    if t > 1: t = 1
    dist2 = (xi - x0 - t*dx)**2 + (yi - y0 - t*dy)**2 + (zi - z0 - t*dz)**2

    return dist2 ** .5

def douglas(st, tolerance=.001, _first=True):
    """\
Perform Douglas-Peucker simplification on the path 'st' with the specified
tolerance.  The '_first' argument is for internal use only.

The Douglas-Peucker simplification algorithm finds a subset of the input
points whose path is never more than 'tolerance' away from the original
input path.
"""
    if len(st) == 1:
        yield st[0]
        return

    l1 = st[0]
    l2 = st[-1]
    
    worst_dist = 0
    worst = 0
    
    for i, p in enumerate(st): 
        if p is l1 or p is l2: continue
        dist = dist_lseg(l1, l2, p)
        if dist > worst_dist:
            worst = i
            worst_dist = dist
            
    if _first: yield st[0]
    if worst_dist > tolerance:
        for i in douglas(st[:worst+1], tolerance, False):
            yield i
        yield st[worst]
        for i in douglas(st[worst:], tolerance, False):
            yield i
    if _first: yield st[-1]


class Gcode:
    "For creating rs274ngc files"
    def __init__(self, homeheight = 1.5, safetyheight = 0.04, tolerance=0.001,
            target=lambda s: sys.stdout.write(s + "\n")):
        self.lastx = self.lasty = self.lastz = self.lasta = None
        self.lastgcode = self.lastfeed = None
        self.homeheight = homeheight
        self.safetyheight = self.lastz = safetyheight
        self.tolerance = tolerance
        self.cuts = []
        self.write = target
        self.time = 0

    def begin(self):
	"""\
This function moves to the safety height, sets many modal codes to default
values, turns the spindle on at 1000RPM, and waits for it to come up to
speed."""
	self.write("G20")
        self.write("G0 Z%.4f" % (self.safetyheight))
	self.write("G17 G40 G49")
	self.write("G54 G80 G90 G94")
        self.write("S1000 M3")
	self.write("G04 P3")

    def flush(self):
	"""\
If any 'cut' moves are stored up, send them to the simplification algorithm
and actually output them.

This function is usually used internally (e.g., when changing from a cut
to a rapid) but can be called manually as well.  For instance, when
a contouring program reaches the end of a row, it may be desirable to enforce
that the last 'cut' coordinate is actually in the output file, and it may
give better performance because this means that the simplification algorithm
will examine fewer points per run."""
        if not self.cuts: return
        for x, y, z in douglas(self.cuts, self.tolerance):
            self.move_common(x, y, z, gcode="G1")
        self.cuts = []

    def end(self):
	"""End the program"""
        self.flush()
        self.safety()
        self.write("M2")

    def exactpath(self):
	"""\
Set exact path mode.  Note that unless self.tolerance is set to zero,
the simplification algorithm may still skip over specified points."""
        self.write("G61")

    def continuous(self):
	"Set continuous mode."
        self.write("G64")

    def rapid(self, x=None, y=None, z=None, a=None):
	"Perform a rapid move to the specified coordinates"
        self.flush()
        self.move_common(x, y, z, a, "G0")

    def move_common(self, x=None, y=None, z=None, a=None, gcode="G0"):
	"An internal function used for G0 and G1 moves"
        gcodestring = xstring = ystring = zstring = astring = ""
        if x == None: x = self.lastx
        if y == None: y = self.lasty
        if z == None: z = self.lastz
        if a == None: a = self.lasta
        if gcode != self.lastgcode:
                gcodestring = gcode
                self.lastgcode = gcode
        if x != self.lastx:
                xstring = " X%.4f" % (x)
                self.lastx = x
        if y != self.lasty:
                ystring = " Y%.4f" % (y)
                self.lasty = y
        if z != self.lastz:
                zstring = " Z%.4f" % (z)
                self.lastz = z
        if a != self.lasta:
                astring = " A%.4f" % (a)
                self.lasta = a
        cmd = "".join([gcodestring, xstring, ystring, zstring, astring])
        if cmd:
            self.write(cmd)

    def set_feed(self, feed):
	"Set the feed rate to the given value"
        self.flush()
        self.write("F%.4f" % feed)

    def cut(self, x=None, y=None, z=None):
	"Perform a cutting move at the specified feed rate to the specified coordinates"
        if self.cuts:
            lastx, lasty, lastz = self.cuts[-1]
        else:
            lastx, lasty, lastz = self.lastx, self.lasty, self.lastz
        if x is None: x = lastx
        if y == None: y = lasty
        if z == None: z = lastz
        self.cuts.append([x,y,z])

    def home(self):
	"Go to the 'home' height at rapid speed"
        self.flush()
        self.rapid(z=self.homeheight)

    def safety(self):
	"Go to the 'safety' height at rapid speed"
        self.flush()
        self.rapid(z=self.safetyheight)


