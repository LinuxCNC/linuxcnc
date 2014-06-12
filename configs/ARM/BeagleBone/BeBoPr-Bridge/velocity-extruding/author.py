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

#    Modified by Frank Tkalcevic (frank@franksworkshop.com.au) to be used as part 
#    of the g1tog23 conversion program.
#

import sys, math

def log(*args, **kwargs):
    if False:
        for arg in args:
            print arg,
        print

def dist_lseg(l1, l2, p):
    "Compute the 3D distance from the line segment l1..l2 to the point p."
    x0, y0, z0, a0, f0 = l1
    xa, ya, za, aa, fa = l2
    xi, yi, zi, ai, fi = p

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

    #log(  "rad1", x1, y1, x2, y2, x3, y3 )
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
    #log( "cent1", P1, P2, P3, Pc )
    #log( "\t", alpha, beta, gamma )
    return Pc.x, Pc.y

def arc_center(plane, p1, p2, p3):
    x1, y1, z1, a1, f1 = p1
    x2, y2, z2, a2, f2 = p2
    x3, y3, z3, a3, f3 = p3
    
    if plane == 17: return cent1(x1,y1,x2,y2,x3,y3)
    if plane == 18: return cent1(x1,z1,x2,z2,x3,z3)
    if plane == 19: return cent1(y1,z1,y2,z2,y3,z3)
    
def arc_rad(plane, P1, P2, P3):
    if plane is None: return sys.maxint

    x1, y1, z1, a1, f1 = P1
    x2, y2, z2, a2, f2 = P2
    x3, y3, z3, a3, f3 = P3
    
    if plane == 17: return rad1(x1,y1,x2,y2,x3,y3)
    if plane == 18: return rad1(x1,z1,x2,z2,x3,z3)
    if plane == 19: return rad1(y1,z1,y2,z2,y3,z3)
    return None, 0

def get_pts(plane, (x,y,z,a,f)):
    if plane == 17: return x,y
    if plane == 18: return x,z
    if plane == 19: return y,z

def one_quadrant(plane, c, p1, p2, p3):
    xc, yc = c
    x1, y1 = get_pts(plane, p1)
    x2, y2 = get_pts(plane, p2)
    x3, y3 = get_pts(plane, p3)
    log( ";one_quadrant=", [xc,yc], [x1,y1], [x2,y2], [x3,y3] )

    def sign(x):
	if abs(x) < 1e-5: return 0
	if x < 0: return -1
	return 1

    signs = set((
	(sign(x1-xc),sign(y1-yc)),
	(sign(x2-xc),sign(y2-yc)),
	(sign(x3-xc),sign(y3-yc))
    ))

    log( ";signs=", signs )
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

    log( "; signs return None ", signs )

def make2pi( theta ):
    while theta < 0:
	theta = theta + 2 * math.pi
    while theta > 2 * math.pi:
	theta = theta - 2 * math.pi
    return theta

# return true if CCW
def arc_dir(plane, c, p1, p2, p3):
    xc, yc = c
    x1, y1 = get_pts(plane, p1)
    x2, y2 = get_pts(plane, p2)
    x3, y3 = get_pts(plane, p3)

    theta_start = make2pi(math.atan2(y1-yc, x1-xc))
    theta_mid = make2pi(math.atan2(y2-yc, x2-xc))
    theta_end = make2pi(math.atan2(y3-yc, x3-xc))
    log( ";arc_dir", theta_start, theta_mid, theta_end )

    return is_arc_ccw( theta_start, theta_mid, theta_end )


#test 0.5, 0.6, 1, CCW = CCW
#test 1, 0.6, 0.5, CW = CW
#test 5, 5.5, 0.5, CCW = CCW
#test 5, 0.1, 0.5, CCW = CW
#test 0.5, 5.5, 5, CW = CCW
#test 0.5, 0.1, 5.5, CW = CW
#test 0.5, 1, 5, CCW = CCW
#test 5, 1, 0.5, CW = CW
#test 5.5, 0.5, 5, CCW = CW
#test 5.5, 5.6, 5, CCW = CCW
#test 5, 4, 6, CW = CW
#test 5, 6, 5.5, CW = CCW


def is_arc_ccw( start, mid, end ):

    if start < end:
        if start < mid and mid < end:
            ret = True
        else:
            ret = False
    else:
        if end < mid and mid < start:
            ret = False
        else:
            ret = True
         
    #log( ";is_arc_ccw ", "CCW" if ret else "CW" )
    return ret

def test_arcs():
    def test(s,m,e,ccw):
        is_ccw = is_arc_ccw( s, m, e)
        #log( "test " + str(s) + ", " + str(m) + ", " + str(e) + ", " + ("CCW" if ccw else "CW") + " = " + ("CCW" if is_ccw else "CW") )
        if is_ccw != ccw:
            log( "test " + str(s) + ", " + str(m) + ", " + str(e) + ", " + ("CCW" if ccw else "CW") + " = " + ("CCW" if is_ccw else "CW") )
            sys.abort

    test(.5,.6,1,True)
    test(1,.6,.5,False)
    test(5, 5.5, .5,True)
    test(5, .1, .5,True)
    test(.5, 5.5, 5,False)
    test(.5, .1, 5.5,False)
    test(.5, 1, 5,True)
    test(5, 1, .5,False)
    test(5.5, .5, 5,True)
    test(5.5, 5.6, 5,True)
    test(5, 4, 6,False)
    test(5, 6, 5.5,False)

def chord_length(plane, c, p1, p2, p3):
    xc, yc = c
    x1, y1 = get_pts(plane, p1)
    x2, y2 = get_pts(plane, p3)
    r = math.hypot(y1-yc, x1-xc)

    theta_start = make2pi(math.atan2(y1-yc, x1-xc))
    theta_end  = make2pi(math.atan2(y2-yc, x2-xc))

    if arc_dir(plane, c, p1, p2, p3 ):
        #CCW
        angle = make2pi(theta_end - theta_start)
    else:
        angle = make2pi(theta_start - theta_end)
        
    return angle * r

def arc_fmt(plane, c1, c2, p1):
    x, y, z, a, f = p1
    if plane == 17: return "I%.4f J%.4f" % (c1-x, c2-y)
    if plane == 18: return "I%.4f K%.4f" % (c1-x, c2-z)
    if plane == 19: return "J%.4f K%.4f" % (c1-y, c2-z)

def douglas(st, tolerance=.001, length_tolerance=0.005, plane=None, _first=True):
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

    test_arcs()

    if len(st) == 1:
        path = []
        path.append( ["G1", st[0], None] )
        return path

    # To build fuller arcs, we will use brute force, appending one
    # point at a time until we no longer have an arc/line
    path = []
    last_path = []
    new_path = []

    log( "; len(st)=", len(st) )
    for p in st:
        log( ";" , p )

    for i_st, point_st in enumerate(st):

        path.append( point_st )

        log( ";path" )
        for n in path:
            log( ";\t", n )

        if i_st == 0:
            continue

        l1 = path[0]
        l2 = path[-1]
    
        worst_dist = 0
        worst = 0
        min_rad = sys.maxint
        max_arc = -1
        path_length = 0     # remember the path length

        ps = path[0]
        pe = path[-1]

        for i, p in enumerate(path): 
            
            if i > 0:
                path_length = path_length + math.hypot(p[0]-path[i-1][0], p[1]-path[i-1][1])

            if p is l1 or p is l2: continue
            dist = dist_lseg(l1, l2, p)
            #log( "dist", i, p, dist )
            if dist > worst_dist:
                worst = i
                worst_dist = dist
                rad = arc_rad(plane, ps, p, pe)
                log( ";rad", rad, max_arc, min_rad )
                if rad < min_rad:
                    max_arc = i
                    min_rad = rad

        worst_arc_dist = 0
        log( ";min_rad=", min_rad )
        if min_rad != sys.maxint:
            c1, c2 = arc_center(plane, ps, path[max_arc], pe)
            log( ";arc center", c1, c2 )
            lx, ly, lz, la, lf = path[0]
            is_one_quadrant = one_quadrant(plane, (c1, c2), ps, path[max_arc], pe)
            log( ";is_one_quadrant=", is_one_quadrant )
            if True: #is_one_quadrant:
                for i, (x,y,z,a,f) in enumerate(path):
                    if plane == 17: dist = abs(math.hypot(c1-x, c2-y) - min_rad)
                    elif plane == 18: dist = abs(math.hypot(c1-x, c2-z) - min_rad)
                    elif plane == 19: dist = abs(math.hypot(c1-y, c2-z) - min_rad)
                    else: dist = sys.maxint
                    log( ";wad", dist, worst_arc_dist )
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

        log( ";worst\t", worst_arc_dist, worst_dist )
        if worst_arc_dist != sys.maxint:
            log( ";douglas", len(st), "\n;\t", path[0], "\n;\t", path[max_arc], "\n;\t", path[-1] )
            log( ";\t", worst_arc_dist, worst_dist )
            log( ";\t", c1, c2 )
            log( ";chord_length", chord_length(plane,(c1,c2),ps,path[max_arc], pe) )
            log( ";path_length", path_length )
        if worst_arc_dist < tolerance and worst_arc_dist < worst_dist and abs(chord_length(plane,(c1,c2),ps,path[max_arc], pe) - path_length) < (len(path)-1)*length_tolerance :
            log( "; arc" )
            log( ";chord_length", chord_length(plane,(c1,c2),ps,path[max_arc], pe) )
            log( ";path_length", path_length )
            log( ";  (len(path)-1)*length_tolerance = ", (len(path)-1)*length_tolerance );
            log ("; abs(chord_length(plane,(c1,c2),ps,path[max_arc], pe) - path_length) = ", abs(chord_length(plane,(c1,c2),ps,path[max_arc], pe) - path_length) )
            ccw = arc_dir(plane, (c1, c2), ps, path[max_arc], pe)
            #if plane == 18: ccw = not ccw # wtf?
            last_path = []
            last_path.append( ["G1", ps, None] )
            if ccw:
                last_path.append(["G3", path[-1], arc_fmt(plane, c1, c2, ps)])
            else:
                last_path.append(["G2", path[-1], arc_fmt(plane, c1, c2, ps)])
        elif worst_dist > tolerance:
            log( "; broke last" )
            for p in last_path:
                new_path.append( p )
            last_path = []
            path = []
            path.append( point_st )
        else:
            log( "; line" )
            last_path = []
            last_path.append( ["G1", path[0], None])
            last_path.append(["G1", path[-1], None])

    if len(last_path) > 0:
        for p in last_path:
            new_path.append( p )
    elif len(path) > 0:
        new_path.append( ["G1", path[0], None] )

    return new_path

