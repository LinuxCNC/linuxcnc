#    This is a component of emc2
#    Copyright 2007 Jeff Epler <jepler@unpythonic.net>
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import sys, math

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

def rad1(x1,y1,x2,y2,x3,y3):
    x12 = x1-x2
    y12 = y1-y2
    x23 = x2-x3
    y23 = y2-y3
    x31 = x3-x1
    y31 = y3-y1

    den = abs(x12 * y23 - x23 * y12)
    if abs(den) < 1e-5: return sys.maxint

    #print "rad1", x1, y1, x2, y2, x3, y3
    math.hypot(x12, y12) * math.hypot(x23, y23) * math.hypot(x31, y31) / 2 / den
    return math.hypot(x12, y12) * math.hypot(x23, y23) * math.hypot(x31, y31) / 2 / den

class Point:
    def __init__(self, x, y):
	self.x = x
	self.y = y
    def __str__(self): return "<%f,%f>" % (self.x, self.y)
    def __sub__(self, other):
	return Point(self.x - other.x, self.y - other.y)	
    def __add__(self, other):
	return Point(self.x + other.x, self.y + other.y)	
    def __mul__(self, other):
	return Point(self.x * other, self.y * other)
    __rmul__ = __mul__
    def cross(self, other):
	return self.x * other.y - self.y * other.x
    def dot(self, other):
	return self.x * other.x + self.y * other.y
    def mag(self):
	return hypot(self.x, self.y)
    def mag2(self):
	return self.x**2 + self.y**2

def cent1(x1,y1,x2,y2,x3,y3):
    P1 = Point(x1,y1)
    P2 = Point(x2,y2)
    P3 = Point(x3,y3)

    den = abs((P1-P2).cross(P2-P3))
    if abs(den) < 1e-5: return sys.maxint, sys.maxint

    alpha = (P2-P3).mag2() * (P1-P2).dot(P1-P3) / 2 / den / den
    beta  = (P1-P3).mag2() * (P2-P1).dot(P2-P3) / 2 / den / den
    gamma = (P1-P2).mag2() * (P3-P1).dot(P3-P2) / 2 / den / den

    Pc = alpha * P1 + beta * P2 + gamma * P3
    #print >>sys.stderr, "cent1", P1, P2, P3, Pc
    #print >>sys.stderr, "\t", alpha, beta, gamma
    return Pc.x, Pc.y

def arc_center(plane, p1, p2, p3):
    x1, y1, z1 = p1
    x2, y2, z2 = p2
    x3, y3, z3 = p3
    
    if plane == 17: return cent1(x1,y1,x2,y2,x3,y3)
    if plane == 18: return cent1(x1,z1,x2,z2,x3,z3)
    if plane == 19: return cent1(y1,z1,y2,z2,y3,z3)
    
def arc_rad(plane, P1, P2, P3):
    if plane is None: return sys.maxint

    x1, y1, z1 = P1
    x2, y2, z2 = P2
    x3, y3, z3 = P3
    
    if plane == 17: return rad1(x1,y1,x2,y2,x3,y3)
    if plane == 18: return rad1(x1,z1,x2,z2,x3,z3)
    if plane == 19: return rad1(y1,z1,y2,z2,y3,z3)
    return None, 0

def get_pts(plane, (x,y,z)):
    if plane == 17: return x,y
    if plane == 18: return x,z
    if plane == 19: return y,z

def one_quadrant(plane, c, p1, p2, p3):
    xc, yc = c
    x1, y1 = get_pts(plane, p1)
    x2, y2 = get_pts(plane, p2)
    x3, y3 = get_pts(plane, p3)

    def sign(x):
	if abs(x) < 1e-5: return 0
	if x < 0: return -1
	return 1

    signs = set((
	(sign(x1-xc),sign(y1-yc)),
	(sign(x2-xc),sign(y2-yc)),
	(sign(x3-xc),sign(y3-yc))
    ))

    if len(signs) == 1: return True
    
    if (1,1) in signs:
	signs.discard((1,0))
	signs.discard((0,1))
    if (1,-1) in signs:
	signs.discard((1,0))
	signs.discard((0,-1))
    if (-1,1) in signs:
	signs.discard((-1,0))
	signs.discard((0,1))
    if (-1,-1) in signs:
	signs.discard((-1,0))
	signs.discard((0,-1))

    if len(signs) == 1: return True

def arc_dir(plane, c, p1, p2, p3):
    xc, yc = c
    x1, y1 = get_pts(plane, p1)
    x2, y2 = get_pts(plane, p2)
    x3, y3 = get_pts(plane, p3)

    theta_start = math.atan2(y1-yc, x1-xc)
    theta_mid = math.atan2(y2-yc, x2-xc)
    theta_end = math.atan2(y3-yc, x3-xc)

    if theta_mid < theta_start:
	theta_mid = theta_mid + 2 * math.pi
    while theta_end < theta_mid:
	theta_end = theta_end + 2 * math.pi

    return theta_end < 2 * math.pi

def arc_fmt(plane, c1, c2, p1):
    x, y, z = p1
    if plane == 17: return "I%.4f J%.4f" % (c1-x, c2-y)
    if plane == 18: return "I%.4f K%.4f" % (c1-x, c2-z)
    if plane == 19: return "J%.4f K%.4f" % (c1-y, c2-z)

def douglas(st, tolerance=.001, plane=None, _first=True):
    """\
Perform Douglas-Peucker simplification on the path 'st' with the specified
tolerance.  The '_first' argument is for internal use only.

The Douglas-Peucker simplification algorithm finds a subset of the input points
whose path is never more than 'tolerance' away from the original input path.

If 'plane' is specified as 17, 18, or 19, it may find helical arcs in the given
plane in addition to lines.  Note that if there is movement in the plane
perpendicular to the arc, it will be distorted, so 'plane' should usually
be specified only when there is only movement on 2 axes
"""
    if len(st) == 1:
        yield "G1", st[0], None
        return

    l1 = st[0]
    l2 = st[-1]
    
    worst_dist = 0
    worst = 0
    min_rad = sys.maxint
    max_arc = -1

    ps = st[0]
    pe = st[-1]

    for i, p in enumerate(st): 
        if p is l1 or p is l2: continue
        dist = dist_lseg(l1, l2, p)
        if dist > worst_dist:
            worst = i
            worst_dist = dist
	    rad = arc_rad(plane, ps, p, pe)
	    #print >>sys.stderr, "rad", rad, max_arc, min_rad
	    if rad < min_rad:
		max_arc = i
		min_rad = rad

    worst_arc_dist = 0
    if min_rad != sys.maxint:
	c1, c2 = arc_center(plane, ps, st[max_arc], pe)
	lx, ly, lz = st[0]
	if one_quadrant(plane, (c1, c2), ps, st[max_arc], pe):
	    for i, (x,y,z) in enumerate(st):
		if plane == 17: dist = abs(math.hypot(c1-x, c2-y) - min_rad)
		elif plane == 18: dist = abs(math.hypot(c1-x, c2-z) - min_rad)
		elif plane == 19: dist = abs(math.hypot(c1-y, c2-z) - min_rad)
		else: dist = sys.maxint
		#print >>sys.stderr, "wad", dist, worst_arc_dist
		if dist > worst_arc_dist: worst_arc_dist = dist

		mx = (x+lx)/2
		my = (y+ly)/2
		mz = (z+lz)/2
		if plane == 17: dist = abs(math.hypot(c1-mx, c2-my) - min_rad)
		elif plane == 18: dist = abs(math.hypot(c1-mx, c2-mz) - min_rad)
		elif plane == 19: dist = abs(math.hypot(c1-my, c2-mz) - min_rad)
		else: dist = sys.maxint
		#if dist > worst_arc_dist: worst_arc_dist = dist

		lx, ly, lz = x, y, z
	else:
	    worst_arc_dist = sys.maxint

    else:
	worst_arc_dist = sys.maxint

    #if worst_arc_dist != sys.maxint:
	#print >>sys.stderr, "douglas", len(st), "\n\t", st[0], "\n\t", st[max_arc], "\n\t", st[-1]
	#print >>sys.stderr, "\t", worst_arc_dist, worst_dist
	#print >>sys.stderr, "\t", c1, c2
    if worst_arc_dist < tolerance and worst_arc_dist < worst_dist:
	ccw = arc_dir(plane, (c1, c2), ps, st[max_arc], pe)
	if plane == 18: ccw = not ccw # wtf?
	yield "G1", ps, None
	if ccw:
	    yield "G3", st[-1], arc_fmt(plane, c1, c2, ps)
	else:
	    yield "G2", st[-1], arc_fmt(plane, c1, c2, ps)
    elif worst_dist > tolerance:
	if _first: yield "G1", st[0], None
        for i in douglas(st[:worst+1], tolerance, plane, False):
            yield i
        yield "G1", st[worst], None
        for i in douglas(st[worst:], tolerance, plane, False):
            yield i
	if _first: yield "G1", st[-1], None
    else:
	if _first: yield "G1", st[0], None
	if _first: yield "G1", st[-1], None

class Gcode:
    "For creating rs274ngc files"
    def __init__(self, homeheight = 1.5, safetyheight = 0.04, tolerance=0.001,
            spindle_speed=1000, units="G20",
            target=lambda s: sys.stdout.write(s + "\n")):
        self.lastx = self.lasty = self.lastz = self.lasta = None
        self.lastgcode = self.lastfeed = None
        self.homeheight = homeheight
        self.safetyheight = self.lastz = safetyheight
        self.tolerance = tolerance
        self.units = units
        self.cuts = []
        self.write = target
        self.time = 0
        self.spindle_speed = spindle_speed
	self.plane = None

    def set_plane(self, p):
	assert p in (17,18,19)
	if p != self.plane:
	    self.plane = p
	    self.write("G%d" % p)

    def begin(self):
	"""\
This function moves to the safety height, sets many modal codes to default
values, turns the spindle on at 1000RPM, and waits for it to come up to
speed."""
	self.write(self.units)
        self.write("G0 Z%.4f" % (self.safetyheight))
	self.write("G17 G40")
	self.write("G80 G90 G94")
        self.write("S%d M3" % (self.spindle_speed))
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
        for move, (x, y, z), cent in douglas(self.cuts, self.tolerance, self.plane):
	    if cent:
		self.write("%s X%.4f Y%.4f Z%.4f %s" % (move, x, y, z, cent))
		self.lastgcode = None
		self.lastx = x
		self.lasty = y
		self.lastz = z
	    else:
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

    def continuous(self, tolerance=0.0):
	"Set continuous mode."
        if tolerance > 0.0:
            self.write("G64 P%.4f" % tolerance)
        else:
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
	if xstring == ystring == zstring == astring == "":
	    return
        if gcode != self.lastgcode:
                gcodestring = gcode
                self.lastgcode = gcode
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
        if y is None: y = lasty
        if z is None: z = lastz
        self.cuts.append([x,y,z])

    def home(self):
	"Go to the 'home' height at rapid speed"
        self.flush()
        self.rapid(z=self.homeheight)

    def safety(self):
	"Go to the 'safety' height at rapid speed"
        self.flush()
        self.rapid(z=self.safetyheight)


