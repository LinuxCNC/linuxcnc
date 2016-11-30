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
    min_depth =  0     # minimum depth of profile
    miny =  0.0   # minimum y coordinate of profile
    mode = -1     # rapid/feed mode
    x_begin = 0.0
    y_begin = 0.0
    x_end = 0.0
    y_end = 0.0

d = data()

# Direction-aware versions of > < >= <=
def GTx(a,b):
    return (a > b) if x_dir > 0 else (a < b)
def LTx(a,b):
    return (a < b) if x_dir > 0 else (a > b)
def GTEx(a,b):
    return (a >= b) if x_dir > 0 else (a <= b)
def LTEx(a,b):
    return (a >= b) if x_dir > 0 else (a <= b)

def nearly_equal(x, y):
    return True if abs(x - y) < 1e-8 else False

def add_to_list(d, mode, ps, qs, pe, qe, r, pc, qc):
    #static double oldx for C++ version
    global oldx
    oldp = d.pmax

    # Spot "peaks" to change pocket numbers
    if GTEx(oldx, ps) and LTx(pe,ps): d.pmax += 1

    # populate the previous segment with this segment's pocket number
    if len(d.list) > 0:
        t0, t1, t2, t3, t4, t5, t6, t7, t8, dummy, pocket = d.list.pop()
        d.list.append((t0, t1, t2, t3, t4, t5, t6, t7, t8, d.pmax, pocket))
        print d.list[-1]
        
    d.list.append((mode, ps, qs, pe, qe, r, pc, qc, oldp, 0, d.pmax))
    if LTx(d.min_depth, ps): d.min_depth = ps
    if LTx(d.min_depth, pe): d.min_depth = pe
    if GTx(d.max_depth, ps): d.max_depth = ps
    if GTx(d.max_depth, pe): d.max_depth = pe
    print "max", d.max_depth, "min", d.min_depth
    # This needs a similar directionality fix
    d.miny = min(d.miny, qs, qe)
    oldx = ps

oldx = 0

def g7x(self, g7xmode, **words):
    global x_dir
    d.list = []
    d.mode = -1
    d.pmax = 1
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
    
    # d.list is a list of lines and arcs. Arcs are split at the cardinal points
    # d.list[n][0] = motion mode (1,2,3)
    # d.list[n][1] = start P (code variable x)
    # d.list[n][2] = start Q (code variable y)
    # d.list[n][3] = end P
    # d.list[n][4] = end Q
    # d.list[n][5] = arc radius
    # d.list[n][6] = arc centre P
    # d.list[n][7] = arc centre Q
    # d.list[n][8] = last x  (May be unused, CHECK)
    # d.list[n][9] = next x (for dealing properly with passing through a node)
    # d.list[n][10]= pocket number
    
    # X-allowance is added as a profile shift.
    # Z-allowance as a delta to cut start and end

    mode = 0
    x, y, x0, y0 = 0, 0, 0, 0

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

    for block in gcode:
        cmds = dict(re.findall('(\w)\s*([-+,\.\d]+)', block.upper(), re.S | re.I))

        if P in cmds:
            x = float(cmds[P])
            print "x = ", x
            if self.params['_lathe_diameter_mode']: x = x / 2
            del cmds[P]
        if Q in cmds:
            y = float(cmds[Q])
            print "y  ", y
            del cmds[Q]

        if 'G' in cmds:
            if cmds['G'] in ('0', '1', '2', '3', '00', '01', '02', '03'):
                mode = int(cmds['G'])
            else:
                print "invalid G-code G%s" % cmds['G']
                exit()
            del cmds['G']

            if d.mode < 0: # Then this is the first block
                # By convention the first move of the block defines the increment path. 
                d.mode = mode
                d.x_end = x
                d.y_end = y
                x_dir = 1 if d.x_end < d.x_begin else -1
                print "direction set to ", x_dir, "d.x_end", d.x_end, "d.x_begin", d.x_begin
                d.Dword = d.Dword * -x_dir #slightly inconsistent sign/dir perhaps?
                d.min_depth =  0
                d.max_depth =  1e9 * x_dir 
                x0 = x
                y0 = y
                continue

        if mode == 0:
            x0 = x
            y0 = y
        elif mode == 1:
            add_to_list(d, 1, x0 + d.Jword, y0, x + d.Jword, y, 0, 0, 0)
        elif mode in (2,3):
            #calculate centre point and radius for all arc styles
            delta = math.sqrt((x - x0)**2 + (y - y0)**2)
            # e is the winding direction
            e = 1 if mode == 2 else -1  
            e = e * flip         
            if 'R' in cmds:
                if 'I' in cmds or 'J' in cmds or 'K' in cmds:
                    print "I J K and R are mutually exclusive"
                    exit()
                #http://math.stackexchange.com/questions/27535
                r = float(cmds['R'])
                del cmds['R']
                u = (x - x0) / delta
                v = (y - y0) / delta
                if delta > 2 * r:
                    print "Circle radius too small for end points"
                    return INTERP_ERROR
                h = math.sqrt(r**2 - (delta**2 / 4))
                
                # negative R means choose the alternate arc
                # but negative R is troublesome later
                if r < 0:
                    r = abs(r)
                    xc = (x0 + x) / 2 + e * h * v
                    yc = (y0 + y) / 2 - e * h * u
                else:
                    xc = (x0 + x) / 2 - e * h * v
                    yc = (y0 + y) / 2 + e * h * u
                
            else:
                G901 = not self.params['_ijk_absolute_mode']
                if I in cmds:
                    xc = x0 + float(cmds[I]) if G901 else float(cmds[I])
                    del cmds[I]
                if J in cmds:
                    yc = y0 + float(cmds[J]) if G901 else float(cmds[J])
                    del cmds[J]
                r = math.sqrt((xc - x0)**2 + (yc - y0)**2)
                r2 = math.sqrt((xc - x)**2 + (yc - y)**2)
                if abs(r - r2) > 0.001:
                    print "impossible arc centre", r, r2
                    exit()
            
            # add cardinal points to arcs
            # There is scope for some confusion here about directions. 
            # Lathe G-code has G2 (CW) and G3 (ACW) as viewed along positive Y
            # Plotting these curves to check points it is clear that ATAN2(+, +)
            # returns positive angles despite that this is anticlockwise looking
            # along positive Z
            # So, in the geometry here, CW and ACW are reversed compared to the 
            # G2 G3 convention. 
            
            a1 = math.atan2(x0 - xc, y0 - yc)
            a2 = math.atan2(x - xc, y - yc)
            
            d90 = math.pi/2
            if e > 0: # G2 arc Anticlocwise in this CS
                nesw = d90 * math.ceil(a1 / d90) - d90
                if a2 > a1:
                    a2 -= 2 * math.pi
                while nesw > a2:
                    x1 = xc + r * math.sin(nesw)
                    y1 = yc + r * math.cos(nesw)
                    add_to_list(d, mode, x0 + d.Jword, y0, x1 + d.Jword, y1, r, xc + d.Jword, yc)
                    x0, y0 = x1, y1
                    nesw -= d90
                    
            elif e < 0: # G3 arc Clockwise in this CS
                nesw = d90 * math.floor(a1 / d90) + d90
                if a2 < a1:
                    a2 += 2 * math.pi
                while nesw < a2:
                    x1 = xc + r * math.sin(nesw)
                    y1 = yc + r * math.cos(nesw)
                    add_to_list(d, mode, x0 + d.Jword, y0, x1 + d.Jword, y1, r, xc + d.Jword, yc)
                    x0, y0 = x1, y1
                    nesw += d90
            else:
                print "E == 0 for an arc?"
                exit()
                
            add_to_list(d, mode, x0 + d.Jword, y0, x + d.Jword, y, r, xc + d.Jword, yc)
                 
        if len(cmds) > 0:
            print "invalid words in block ", cmds
        
        # Starting point of next segment
        x0 = x
        y0 = y

    # Make a d.list of cuts of the form:
    # (pocket-sequence, x level, start y, end y)
    # Still to be decided: How to handle a profile that isn't open to the right
    # But probably means initialising current_pocket intelligently
    cuts = []
    x = d.x_begin
    while GTx(x + d.Dword, d.max_depth):

        x = x + d.Dword
        y0 = d.y_begin # current cut-start point

        #decide if we are starting inside or outside the material
        current_pocket = 1 if GTx(x, d.x_end + d.Jword) else 0

        # start by cutting down to the profile
        if GTEx(x, d.min_depth):
            cuts.append((current_pocket, x, d.y_begin, d.miny))
            continue
        
        for block in d.list:
            #Find all lines and arcs which span the feed line

            # Special treatment for hitting a node exactly.
            # when you hit a node exactly it is the start of one segment and the
            # end of another, So we get some zero-length cuts, ignored later
            if nearly_equal(x, block[1]):
                if nearly_equal(x, block[3]): # completly ignore lines parallel to, and on, the cut
                        continue
                y = block[2]
            elif nearly_equal(x, block[3]):
                y = block[4]
            elif ((x > block[1]) != (x > block[3])):
                if block[0] == 1: # G1 move
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
                    # if the conditions are equal then it is the wrong solution
                    dy = math.sqrt(r**2 - (x - xc)**2)
                    y = yc + dy
                    if (y >= block[2]) == (y > block[4]):
                        y = yc - dy
                    if (y >= block[2]) == (y > block[4]):
                        print "Well, that's me stumped"
                        print x, y, block[2], block[4], xc, yc, r
                        bp()
                        continue
                else: 
                    print "what the heck? Not G0, G1 G2 or G3?"
                    return INTERP_ERROR

            else: # Nothing found
                continue

            # At this point we have found an intersection of the cut line
            # with the profile section 'block' at the point x,y
            
            if current_pocket and not nearly_equal(y, y0): # Ignore points on top of each other
                st = y0 - d.Lword if current_pocket > 1 else y0
                en = y + d.Lword
                # skip cuts that are "squeezed-out" by L
                if st > en:
                    cuts.append((current_pocket, x, st, en))
                    print cuts[-1]
                current_pocket = 0
            elif current_pocket == 0:
                y0 = y
                current_pocket = block[10]

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
                pose[axes[P]] = c[1] + d.Rword if x_dir > 0 else c[1] - d.Rword
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
    

