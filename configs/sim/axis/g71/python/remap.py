#! /usr/bin/python

from interpreter import *
import emccanon

from stdglue import cycle_prolog, cycle_epilog, init_stdglue

import linuxcnc
import inspect
import re
import math
import rs274

s = linuxcnc.stat()

## {{{ http://code.activestate.com/recipes/145297/ (r1)
def lineno():
    """Returns the current line number in our program."""
    frame =  inspect.currentframe()
    return frame.f_back.f_lineno

def add_to_list(list, mode, ps, qs, pe, se, r, pc, qc):
    #static double oldx for C++ version
    if len(list) > 0:
        t0, t1, t2, t3, t4, t5, t6, t7, t8, dummy = list.pop()
        list.append((t0, t1, t2, t3, t4, t5, t6, t7, t8, pe))
    list.append((mode, ps, qs, pe, se, r, pc, qc, add_to_list.oldx, 0))
    add_to_list.oldx = ps

add_to_list.oldx = 0

def g7x(self, g7xmode, **words):
    print words
    s.poll()
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

    planes={170:('X','Y','I','J'), 180:('X','Z','I','K'), 190:('Y','Z','J','K'),
            171:('U','V','',''), 181:('U','W','',''), 191:('V','W','','')}
    axes={'X':0, 'Y':1, 'Z':2, 'A':3, 'B':4, 'C':5, 'U':6, 'V':8, 'W':9}

    if g7xmode == 71:
        flip = 1
        P, Q, I, J = planes[self.params['_plane']]
    elif g7xmode == 72:
        flip = -1
        Q, P, J, I = planes[self.params['_plane']]
    else:
        print "well, that's odd"
        exit()

    print P,Q,I,J
    
    # list is a list of lines and arcs. Arcs are split at the cardinal points
    # list[n][0] = motion mode (1,2,3)
    # list[n][1] = start P (code variable x)
    # list[n][2] = start Q (code variable y)
    # list[n][3] = end P
    # list[n][4] = end Q
    # list[n][5] = arc radius
    # list[n][6] = arc centre P
    # list[n][7] = arc centre Q
    # list[n][8] = last x
    # list[n][9] = next x (for dealing properly with passing through a node)

    list = []
    mode = 0
    x, y, x0, y0 = 0, 0, 0, 0

    if 'd' in words:
        dword = abs(words['d'])
    else:
        print "G71/72 error, can't work without D"
        return INTERP_ERROR
    if 'r' in words:
        rword = words['r']
    else:
        rword = dword
    
    for block in gcode:
        x = x0
        y = y0
        cmds = dict(re.findall('(\w)\s*([-+,\.\d]+)', block.upper(), re.S | re.I))
        print cmds
        if 'G' in cmds:
            if cmds['G'] in ('0', '1', '2', '3', '00', '01', '02', '03'):
                mode = int(cmds['G'])
            else:
                print "invalid G-code G%s" % cmds['G']
                exit()
            del cmds['G']
        if P in cmds:
            x = float(cmds[P])
            if self.params['_lathe_diameter_mode']: x = x / 2
            del cmds[P]
        if Q in cmds:
            y = float(cmds[Q])
            del cmds[Q]
            
        if mode == 0:
            x0 = x
            y0 = y
        elif mode == 1:
            add_to_list(list, 1,x0,y0,x,y,0,0,0)
        elif mode in (2,3):
            #calculate centre point and radius for all arc styles
            cx, cy = x0, y0
            d = math.sqrt((x - x0)**2 + (y - y0)**2)
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
                u = (x - x0) / d
                v = (y - y0) / d
                h = math.sqrt(r**2 - (d**2 / 4))
                
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
            print xc, yc, math.degrees(a1), math.degrees(a2)
            
            d90 = math.pi/2
            if e > 0: # G2 arc Anticlocwise in this CS
                nesw = d90 * math.floor(a1 / d90)
                if a2 > a1:
                    a2 -= 2 * math.pi
                while nesw > a2:
                    x1 = xc + r * math.sin(nesw)
                    y1 = yc + r * math.cos(nesw)
                    add_to_list(list, mode, x0, y0, x1, y1, r, xc, yc)
                    x0, y0 = x1, y1
                    nesw -= d90
                    
            elif e < 0: # G3 arc Clockwise in this CS
                nesw = d90 * math.ceil(a1 / d90)
                if a2 < a1:
                    a2 += 2 * math.pi
                while nesw < a2:
                    x1 = xc + r * math.sin(nesw)
                    y1 = yc + r * math.cos(nesw)
                    add_to_list(list, mode, x0, y0, x1, y1, r, xc, yc)
                    x0, y0 = x1, y1
                    nesw += d90
            else:
                print "E == 0 for an arc?"
                exit()
                
            add_to_list(list, mode, x0, y0, x, y, r, xc, yc)
                 
        if len(cmds) > 0:
            print "invalid words in block ", cmds
        
        x0 = x
        y0 = y

    # And now we have a nice list of profiles to analyse. 
    """
    P     Block Number of contour beginning (uses N word in beginning block)
    Q     Block number of contour end (uses N word in end block)
    D     Roughing Depth per move
    R     Retraction from each cut
    J     Overthickness for finishing at X (diameter)   (U on other controllers)
    L     Overthickness for finishing at Z              (W on other controllers)
    I     Thickness for finishing at X
    K     Thickness for finishing at Z
    F     Feedrate override between P and Q blocks
    S     Spindle speed override between P and Q blocks
    T     Tool override between P and Q blocks"""

    s.poll
    pose = [self.params['_X'], self.params['_Y'], self.params['_Z'],
            self.params['_A'], self.params['_B'], self.params['_C'], 
            self.params['_U'], self.params['_V'], self.params['_W']]
    xs = pose[axes[P]]
    ys = pose[axes[Q]]

    # By convention the first move of the block defines the increment path. 
    xe = list[0][1]
    mode = list[0][0]
    
    x = xs
    if xs > xe: dword = -dword
    
    # Make a list of cuts of the form:
    # (pocket-sequence, x level, start y, end y)
    # Still to be decided: How to handle a profile that isn't open to the right
    # But probably means initialising has_start intelligently
    cuts = []
    pcount = 0
    
    while (x >= xe) == ((x + dword) >= xe):
        x = x + dword
        pocket_id = 0
        has_start = True
        y0 = ys # current cut-start point
        for block in list:
            #Find all lines and arcs which span the feed line

            # Special treatment for hitting a node exactly.
            # look for a direction change at this node
            # I think it is actually safe to check for float equality here
            if x == block[1]:
                if (x - block[3]) * (x - block[8]) > 0:
                    continue
            if x == block[3]:
                 if (x - block[1]) * (x - block[9]) > 0:
                    continue

            if (x >= block[1]) != (x > block[3]):
                if block[0] == 1: # G1 move
                    #intercept of cut line with path segment
                    # t is the normalised length along the line
                    if block[1] == block[3]:
                        # This is a spcial case as one segment has to be both 
                        # an exit and entry point.
                        y = block[2]
                        if has_start: pocket_id -= 1
                        has_start = False
                    else:
                        t = (x - block[1])/(block[3] - block[1])
                        y = block[2] + t * (block[4] - block[2])
                elif block[0] in (2, 3): # Arc moves here
                    y1 = None # An arc can't be parallel to the cut
                    # a circle is x^2 + y^2 = r^2
                    # (x - xc)^2 + (y - yc)^2 = r^2
                    # (y - yc) = sqrt(r^2 - (x - xc)^2)
                    # 2 solutions for a line through a circle
                    # because we split the arcs into <= 90 degree sections it 
                    # is easy to check which solution is between the arc ends
                    r = block[5]
                    xc = block[6]
                    yc = block[7]
                    # if the conditions are equal then it's the wrong solution
                    dy = math.sqrt(r**2 - (x - xc)**2)
                    y = yc + dy
                    if (y >= block[2]) == (y > block[4]):
                        y = yc - dy
                    if (y >= block[2]) == (y > block[4]):
                        print "Well, that's me stumped"
                        print x, y, block[2], block[4]
                        continue
                else: 
                    print "what the heck? Not G0, G1 G2 or G3?"
                    return INTERP_ERROR

                # At this point we have found an intersection of the cut line
                # with the profile at the point x,y
                
                if has_start:
                    cuts.append((pocket_id, x, y0, y))
                    has_start = False
                    pcount = max(pcount, pocket_id)
                else:
                    y0 = y
                    has_start = True
                    pocket_id += 1

    #And now make the cuts
    
    for p in range(0, pcount + 1):
        for c in cuts:
            if c[0] == p:
                pose[axes[Q]] = c[2]
                emccanon.STRAIGHT_TRAVERSE(p, *pose)
                if xs > xe:
                    pose[axes[P]] = c[1] - dword
                else:
                    pose[axes[P]] = c[1] + dword
                emccanon.STRAIGHT_TRAVERSE(p, *pose) 
                pose[axes[P]] = c[1]
                emccanon.STRAIGHT_FEED(p, *pose)
                pose[axes[Q]] = c[3]
                emccanon.STRAIGHT_FEED(p, *pose)
                if xs > xe:
                    pose[axes[P]] = c[1] + rword
                else:
                    pose[axes[P]] = c[1] - rword
                pose[axes[Q]] = c[3] + rword
                emccanon.STRAIGHT_TRAVERSE(p, *pose)
                # pre-load the retract in case end of pocket
                pose[axes[Q]] = c[2]
                
        emccanon.STRAIGHT_TRAVERSE(5, *pose)         
        pose[axes[P]] = xs
        emccanon.STRAIGHT_TRAVERSE(2, *pose)        
          
    pose[axes[P]] = xs
    pose[axes[Q]] = ys    
    emccanon.STRAIGHT_TRAVERSE(2, *pose)    
    return INTERP_OK

def g71(self, **words):
    g7x(self, 71, **words)
    
def g72(self, **words):
    g7x(self, 72, **words)
    

