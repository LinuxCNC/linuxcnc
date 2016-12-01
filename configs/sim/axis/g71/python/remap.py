#! /usr/bin/python

from interpreter import *
import emccanon

from stdglue import cycle_prolog, cycle_epilog, init_stdglue

import linuxcnc
import inspect
import re
import math
import rs274

from pdb import set_trace as bp

s = linuxcnc.stat()

class data:
    # I see this as a C-struct
    list = []
    Pword = 0     # Block Number of contour beginning (uses N word in beginning block)
    Dword = 0     # Roughing Depth per cut
    Rword = 0     # Retraction from each cut
    Jword = 0     # Overthickness before finishing X (diameter)   (U on other controllers)
    Lword = 0     # Overthickness before finishing Z              (W on other controllers)
    Iword = 0     # Thickness for finishing at X
    Kword = 0     # Thickness for finishing at Z
    Fword = 0     # Feedrate override between P and Q blocks
    Sword = 0     # Spindle speed override between P and Q blocks
    Tword = 0     # Tool for cycle
    pmax =  1     # top pocket number
    max_depth =  0.0   # maximum depth of profile
    miny =  0.0   # minimum y coordinate of profile
    mode = -1     # rapid/feed mode

d = data()

# Direction-aware versions of > < >= <= max() min()
def GTx(a,b):
    return (a > b) if d.Dword < 0 else (a < b)
def LTx(a,b):
    return (a < b) if d.Dword < 0 else (a > b)
def GTEx(a,b):
    return (a >= b) if d.Dword < 0 else (a <= b)
def LTEx(a,b):
    return (a >= b) if d.Dword <= 0 else (a <= b)
def MINx(*vals):
    return min(vals) if d.Dword <= 0 else max(vals)
def MAXx(*vals):
    return max(vals) if d.Dword <= 0 else min(vals)
def nearly_equal(x, y):
    return True if abs(x - y) < 1e-8 else False
def between(l1, x, l2):
    if l1 > l2:
        if x >= l2 and x <= l1:
            return True
    else:
        if x >= l1 and x <= l2:
            return True
    return False

    
# add_to_list() is where G-code line segments are added to the list
# Each consists of a set of curve type, beginning and end coordinates 
# and a pocket number. The pocket number is +ve integer on curve segments 
# that are a metal-to-air transition, 0 on air-to-metal
# horizontal line segments are completely ignored

# d.list is a list of lines and arcs. Arcs are split at the cardinal points
# d.list[n][0] = motion mode (1,2,3)
# d.list[n][1] = start P (code variable x)
# d.list[n][2] = start Q (code variable y)
# d.list[n][3] = end P
# d.list[n][4] = end Q
# d.list[n][5] = arc radius
# d.list[n][6] = arc centre P
# d.list[n][7] = arc centre Q
# d.list[n][8]= pocket number

# ****** All logic is written for top-right quadrant for consistency ******

# This code assumes that the cut runs in the same left-right direction as the profile
# if that assumption changes, this code will need to know that. 
 
def add_to_list(d, mode, ps, qs, pe, qe, r, pc, qc):
    #static double oldx for C++ version
    global oldp
    d.max_depth = MINx(d.max_depth, pe + d.Jword, ps + d.Jword)
    # This needs a similar directionality fix
    d.miny = min(d.miny, qs, qe)

    if nearly_equal(ps, pe):
        return # Just ignore horizontal lines
    elif LTx(pe, ps):
        if oldp > 0:
            p = oldp
        else:
            d.pmax += 1
            p = d.pmax
    elif GTx(pe, ps):
        p = 0

    oldp = p

    # X-allowance is added as a profile shift here (including the centre point, even for G0/G1
    # Z-allowance as a delta to cut start and end later
    d.list.append((mode, ps + d.Jword, qs, pe + d.Jword, qe, r, pc + d.Jword, qc, p))
    print d.list[-1]

oldp = 0

def g7x(self, g7xmode, **words):
    global x_dir
    d.list = []
    d.mode = -1
    d.pmax = 0
    d.miny = 1.0e10
    s.poll()

    planes={170:('X','Y','I','J'), 180:('X','Z','I','K'), 190:('Y','Z','J','K'),
            171:('U','V','',''), 181:('U','W','',''), 191:('V','W','','')}
    axes={'X':0, 'Y':1, 'Z':2, 'A':3, 'B':4, 'C':5, 'U':6, 'V':8, 'W':9}

    pose = [self.params['_X'], self.params['_Y'], self.params['_Z'],
            self.params['_A'], self.params['_B'], self.params['_C'], 
            self.params['_U'], self.params['_V'], self.params['_W']]

    regex= r'\O0*%(p)i\s*SUB(.*)O0*%(p)i\s*ENDSUB' % words
    gcode = re.findall(regex,open(s.file).read(), re.M | re.S | re.I)
    if len(gcode) == 0:
        print "G71 reference P %(p)i but O%(p)i SUB not in file" % words
        return INTERP_ERROR
    gcode = gcode[0].split('\n')

    # Be agnostic about plane. 
    # P and Q are the plane coordinate letters I and J the centre offsets
    # These are used in the Regex to find the axis letters
    # for G71 P is increment direction (x), Q is feed direction (y)
    # for G72 Q is increment direction (y), P is feed direction (x)  

    if g7xmode == 71:
        flip = 1
        P, Q, I, J = planes[self.params['_plane']]
    elif g7xmode == 72:
        flip = -1
        Q, P, J, I = planes[self.params['_plane']]
    else:
        print "well, that's odd"
        exit()

    d.x_begin = pose[axes[P]]
    d.y_begin = pose[axes[Q]]

    print P,Q,I,J

    if 'd' in words:
        d.Dword = abs(words['d'])
    else:
        print "G71/72 error, can't work without D"
        return INTERP_ERROR
    if 'r' in words:
        d.Rword = words['r']
    else:
        d.Rword = d.Dword        
    if 'j' in words: d.Jword = words['j']   
    if 'l' in words: d.Lword = words['l']
    if 'i' in words: d.Iword = words['i']
    if 'k' in words: d.Kword = words['k']

    x = d.x_begin
    y = d.y_begin
    d.max_depth = d.x_begin

    mode = -1

    for block in gcode:
        oldx = x
        oldy = y
        cmds = dict(re.findall('(\w)\s*([-+,\.\d]+)', block.upper(), re.S | re.I))

        if P in cmds:
            x = float(cmds[P])
            if self.params['_lathe_diameter_mode']: x = x / 2
        if Q in cmds:
            y = float(cmds[Q])

        if 'G' in cmds:
            if cmds['G'] in ('0', '1', '2', '3', '00', '01', '02', '03'):
                mode = int(cmds['G'])
                if d.mode < 0: # Then this is the first block, and sets the cut direction 
                    d.mode = mode
                    mode = 0 # turn off offsetting on the entry line
                    if x < oldx: d.Dword = -1 * d.Dword
                    print "direction set to ", d.Dword, "oldx", oldx, "new x", x
            else:
                print "invalid G-code G%s" % cmds['G']
                exit()

        if mode in (0, 1):
            add_to_list(d, mode, oldx, oldy, x, y, 0, 0, 0)
        elif mode in (2, 3):
            #calculate centre point and radius for all arc styles
            delta = math.sqrt((x - oldx)**2 + (y - oldy)**2)
            # e is the winding direction
            e = 1 if mode == 2 else -1  
            e = e * flip         
            if 'R' in cmds:
                if 'I' in cmds or 'J' in cmds or 'K' in cmds:
                    print "I J K and R are mutually exclusive"
                    exit()
                #http://math.stackexchange.com/questions/27535
                r = float(cmds['R'])
                u = (x - oldx) / delta
                v = (y - oldy) / delta
                if delta > abs(2 * r):
                    print "Circle radius too small for end points"
                    return INTERP_ERROR
                h = math.sqrt(r**2 - (delta**2 / 4))
                
                # negative R means choose the alternate arc
                # but negative R is troublesome later
                if r < 0:
                    r = abs(r)
                    xc = (oldx + x) / 2 + e * h * v
                    yc = (oldy + y) / 2 - e * h * u
                else:
                    xc = (oldx + x) / 2 - e * h * v
                    yc = (oldy + y) / 2 + e * h * u
                
            else:
                G901 = not self.params['_ijk_absolute_mode']
                if I in cmds:
                    xc = oldx + float(cmds[I]) if G901 else float(cmds[I])
                    del cmds[I]
                if J in cmds:
                    yc = oldy + float(cmds[J]) if G901 else float(cmds[J])
                    del cmds[J]
                r = math.sqrt((xc - oldx)**2 + (yc - oldy)**2)
                r2 = math.sqrt((xc - x)**2 + (yc - y)**2)
                if abs(r - r2) > 0.001:
                    print "impossible arc centre", r, r2
                    return INTERP_ERROR
            
            # add cardinal points to arcs
            # There is scope for some confusion here about directions. 
            # Lathe G-code has G2 (CW) and G3 (ACW) as viewed along positive Y
            # Plotting these curves to check points it is clear that ATAN2(+, +)
            # returns positive angles despite that this is anticlockwise looking
            # along positive Z
            # So, in the geometry here, CW and ACW are reversed compared to the 
            # G2 G3 convention. 
            
            a1 = math.atan2(oldx - xc, oldy - yc)
            a2 = math.atan2(x - xc, y - yc)
            
            d90 = math.pi/2
            if e > 0: # G2 arc Anticlocwise in this CS
                nesw = d90 * math.ceil(a1 / d90) - d90
                if a2 > a1:
                    a2 -= 2 * math.pi
                while nesw > a2:
                    x1 = xc + r * math.sin(nesw)
                    y1 = yc + r * math.cos(nesw)
                    add_to_list(d, mode, oldx, oldy, x1, y1, r, xc, yc)
                    oldx, oldy = x1, y1
                    nesw -= d90
                    
            elif e < 0: # G3 arc Clockwise in this CS
                nesw = d90 * math.floor(a1 / d90) + d90
                if a2 < a1:
                    a2 += 2 * math.pi
                while nesw < a2:
                    x1 = xc + r * math.sin(nesw)
                    y1 = yc + r * math.cos(nesw)
                    add_to_list(d, mode, oldx, oldy, x1, y1, r, xc, yc)
                    oldx, oldy = x1, y1
                    nesw += d90
            else:
                print "E == 0 for an arc?"
                exit()
                
            add_to_list(d, mode, oldx, oldy, x, y, r, xc, yc)
                 

    # Make a d.list of cuts of the form:
    # (pocket-sequence, x level, start y, end y)
    cuts = []
    x = d.x_begin
    while GTx(x + d.Dword, d.max_depth):
        x = x + d.Dword
        y0 = d.y_begin + 1 # current cut-start point
        pocket = -1

        # start by cutting down to the profile
 #       if GTEx(x, d.min_depth):
 #           cuts.append((1, x, d.y_begin, d.miny))
 #           continue
        
        intercept = False
        for block in d.list:
            #[0]G-code [1]xs [2]ys [3]xe [4]ye [5]r [6]xc [7]yc [8]pocket
            #Find all lines and arcs which span the feed line

            # Special treatment for hitting a node exactly.
            # when you hit a node exactly it is the start of one segment and the
            # end of another, So we get some zero-length cuts to ignore
            #if nearly_equal(x, block[1]) and not nearly_equal(y, block[2]):
           #     y = block[2]
            #    intercept = True
            #    print "intercept 1, y = ", y, block
           # elif nearly_equal(x, block[3]) and not nearly_equal(y, block[4]):
           #     y = block[4]
           #     intercept = True
           #     print "intercept 3, y = ", y, block
            if ((x >= block[1]) != (x >= block[3])):
                intercept = True
                if block[0] in (0, 1): #  straight line
                    #intercept of cut line with path segment
                    # t is the normalised length along the line 
                    t = (x - block[1])/(block[3] - block[1])
                    y = block[2] + t * (block[4] - block[2])
                elif block[0] in (2, 3): # Arc moves here
                    # a circle is x^2 + y^2 = r^2
                    # (x - xc)^2 + (y - yc)^2 = r^2
                    # (y - yc) = sqrt(r^2 - (x - xc)^2)
                    # 2 solutions for a line through a circle
                    # because we split the arcs into <= 90 degree sections it 
                    # is easy to check which solution is between the arc ends
                    r = block[5]
                    xc = block[6]
                    yc = block[7]
                    # The "abs" is a sign that there is a problem with rounding somewhere
                    dy = math.sqrt(abs(r**2 - (x - xc)**2))
                    y = yc + dy
                    if (y - block[2]) * (y - block[4]) > 0:
                        y = yc - dy
                    if (y - block[2]) * (y - block[4]) > 0:
                        print "Well, that's me stumped"
                        print x, y, block[2], block[4], xc, yc, r
                        bp()
                        continue
                else: 
                    print "what the heck? Not G0, G1 G2 or G3?"
                    return INTERP_ERROR

            if intercept:
                intercept = False
                #if nearly_equal(y, y0): # Ignore points on top of each otherdy
                #    print y, y0
                #    print "nearly-equal, I thought that problem was gone"
                #    continue
                if block[8] == 0: # end-of pocket intecept
                    y1 = y if block[0] == 0 else y + d.Lword
                    # skip cuts that are "squeezed-out" by L
                    if y0 > y1:
                        cuts.append((pocket, x, y0, y1))
                else:
                    pocket = block[8]
                    y0 = y if block[0] == 0 else y - d.Lword

    #And now make the cuts
    
    for p in range(0, d.pmax + 1):
        for c in cuts:
            if c[0] == p:
                pose[axes[Q]] = c[2]
                emccanon.STRAIGHT_TRAVERSE(p, *pose)
                pose[axes[P]] = c[1] - d.Dword
                emccanon.STRAIGHT_TRAVERSE(p, *pose) 
                pose[axes[P]] = c[1]
                emccanon.STRAIGHT_FEED(p, *pose)
                pose[axes[Q]] = c[3]
                emccanon.STRAIGHT_FEED(p, *pose)
                pose[axes[P]] = c[1] + d.Rword if d.Dword < 0 else c[1] - d.Rword
                pose[axes[Q]] = c[3] + d.Rword
                emccanon.STRAIGHT_TRAVERSE(p, *pose)
                # pre-load the retract in case end of pocket
                pose[axes[Q]] = c[2]
                
        emccanon.STRAIGHT_TRAVERSE(5, *pose)         
        pose[axes[P]] = d.x_begin
        emccanon.STRAIGHT_TRAVERSE(2, *pose)        
          
    pose[axes[P]] = d.x_begin
    pose[axes[Q]] = d.y_begin   
    emccanon.STRAIGHT_TRAVERSE(2, *pose)    
    return INTERP_OK

def g71(self, **words):
    g7x(self, 71, **words)
    
def g72(self, **words):
    g7x(self, 72, **words)
    

