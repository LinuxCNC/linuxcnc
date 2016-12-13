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
    retracts = []
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
    warning = ""

d = data()

# Direction-aware versions of > < >= <= max() min()
def GT(q, a, b):
    return (a > b) if q > 0 else (a < b)
def LT(q, a, b):
    return (a < b) if q > 0 else (a > b)
def GTE(a, b):
    return (q, a >= b) if q > 0 else (a <= b)
def LTE(a,b):
    return (q, a >= b) if q > 0 else (a <= b)
def MIN(q, *vals):
    return min(vals) if q >=0 else max(vals)
def MAX(q, *vals):
    return max(vals) if q >= 0 else min(vals)
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
    #static double oldpok for C++ version
    global oldpok # +ve start of pocket, -ve end of pocket
    pok = 0

    d.max_depth = MIN(-d.Dword, d.max_depth, pe + d.Jword, ps + d.Jword)
    # This needs a similar directionality fix
    d.miny = min(d.miny, qs, qe)

    if nearly_equal(ps, pe):
        return # Just ignore horizontal lines
    elif LT(-d.Dword, pe, ps):
        if oldpok > 0:
            pok = oldpok
        else:
            d.pmax += 1
            pok = d.pmax
            d.retracts.append(MIN(-d.Dword, d.x_begin, ps - d.Dword + d.Jword))
    elif GT(-d.Dword, pe, ps):
        pok = -abs(oldpok)

    oldpok = pok

    # X-allowance is added as a profile shift here (including the centre point, even for G0/G1
    # Z-allowance as a delta to cut start and end later
    d.list.append((mode, ps + d.Jword, qs, pe + d.Jword, qe, r, pc + d.Jword, qc, pok))

#note for translation to C++, global (static) inialisation outside the defining function
oldpok = 0

def find_intercept(block, x):
    if between(block[1], x, block[3]):
        if block[0] in (0, 1): #  straight line
            #intercept of cut line with path segment
            # t is the normalised length along the line 
            t = (x - block[1])/(block[3] - block[1])
            y = block[2] + t * (block[4] - block[2])
            angle = math.atan2(block[3] - block[1], block[4] - block[2]) % math.pi
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
                return 0, 0, 0
            angle = (math.pi/2 - math.atan2(x - xc, yc - y)) % math.pi
        return block[8], y, angle
    return 0, 0, 0

def g7x(self, g7xmode, **words):
    global x_dir
    d.list = []
    d.retracts = []
    d.mode = -1
    d.pmax = 0
    oldpok = 0
    d.miny = 1.0e10
    d.warning = ""
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

    if g7xmode in (710, 711):
        flip = 1
        P, Q, I, J = planes[self.params['_plane']]
    elif g7xmode in (720, 721):
        flip = -1
        Q, P, J, I = planes[self.params['_plane']]
    else:
        print "well, that's odd"
        exit()

    d.x_begin = pose[axes[P]]
    d.y_begin = pose[axes[Q]]
    d.retracts.append(d.x_begin)

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
    if 'l' in words: d.Lword = words['l'] / 1000.0
    if 'i' in words: d.Iword = words['i']
    if 'k' in words: d.Kword = words['k']

    if 't' in words:
        emccanon.CHANGE_TOOL(int(words['t']))
        tool = words['t']
    else:
        tool = self.params[5400] # Not sure why the CHANGE_TOOL above is not effective

    s.poll()

    x = d.x_begin
    y = d.y_begin
    d.max_depth = d.x_begin

    mode = -1

    # This code duplicates the standard LinuxCNC arc and line code, but without tool radius compensation
    # It _should_ call the base LinuxCNC code to assure that G71 gets the same answers as the finishing pass. 

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
            else:
                return "invalid G-code G%s" % cmds['G']

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
                    return "G71/G72: Circle radius too small for end points"

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
                    return "G71: inconsistent arc centre", r, r2
            
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
                 
    # Determine if the profile runs left-right or right-left
    # frontangle/backangle is absolute, so we need to switch them for left-to-right
    # +1 is left-to-right so conventional turning is -1 type. 
    # Use a max(1, ...) to avoid divide by zero with the tan

    s.poll
    if d.list[-1][4] < d.list[0][2]:
        y_dir = -1.0
        frontangle = max(1, self.params[5411])
        backangle = max(1, self.params[5412])
    else:
        y_dir = +1.0
        frontangle = max(1, self.params[5412])
        backangle = max(1, self.params[5411])

    if tool == 0: # no tool
        frontangle = 135
        backangle = 45


    # Make a d.list of cuts of the form:

    print "tool = ", self.params[5400]

    # (pocket-sequence, x level, start y, end y, entry-y, exit-y)
    cuts = []
    x = d.x_begin
    while GT(-d.Dword, x + d.Dword, d.max_depth):
        x = x + d.Dword
        y0 = d.y_begin - y_dir # notional cut-start point, outside the profile
        pocket = -1
        p = 0
        entry = y0
        exit = d.miny

        for k in range(0, len(d.list)):
            block = d.list[k]
            #[0]G-code [1]xs [2]ys [3]xe [4]ye [5]r [6]xc [7]yc [8]pocket
            #Find all lines and arcs which span the feed line

            intercept, y, tanangle = find_intercept(block, x)

            if intercept < 0 and pocket > 0: # end-of pocket intercept
                for j in range(k, len(d.list), 1):
                    p, exit, angle = find_intercept(d.list[j], x - d.Dword)
                    if p != 0:
                        break
                if math.tan(tanangle) != 0:
                    exit = MIN(y_dir, exit, y - d.Dword / math.tan(tanangle))

                    
                if block[0] == 0:
                    y1 = y
                    exit = y
                else:
                    y1 = y - d.Lword * y_dir
                    exit -= d.Lword * y_dir
                # skip cuts that are "squeezed-out" by L
                if LT(y_dir, y0 , y1): 
                    # sometimes we find a second terminator curve further down the profile, so replace the old one
                    if len(cuts) > 0 and cuts[-1][0] == pocket and cuts[-1][1] == x:
                        cuts.pop()
                    cuts.append((pocket, x, y0, y1, entry, exit))
                    # detect gouging, but not for G0 moves
                    cutangle = math.degrees(math.atan2(-d.Dword, (exit - y1))) % 360
                    # In / out swap GT / LT. Left / right swaps GT / LT _and_  front / back  
                    if p != 0 and block[0] != 0 and self.params[5400] != 0 and LT(d.Dword * y_dir, cutangle, frontangle):
                        d.warning = (("G71: The programmed profile has an exit ramp angle of %s and can not be cut "
                                      "with the active tool frontangle of %s") % (cutangle,  frontangle))

            elif intercept > 0 and intercept != pocket: # start of pocket intercept 
                # don't do pockets if Type1 mode has been forced (G71.1, G72.1)
                if g7xmode in (711, 721) and intercept > 1:
                    break
                pocket = intercept
                for j in range(k, 0, -1):
                    p, entry, angle = find_intercept(d.list[j], x - d.Dword)
                    if p != 0:
                        break
                if math.tan(tanangle) != 0:
                    entry = MAX(y_dir, entry, y - d.Dword / math.tan(tanangle))

                
                if block[0] == 0:
                    y0 = y
                    entry = y
                else:
                    y0 = y + d.Lword * y_dir
                    entry += d.Lword * y_dir
                    cutangle = math.degrees(math.atan2(-d.Dword, (entry - y0))) % 360
                    if p != 0 and self.params[5400] != 0 and GT(d.Dword * y_dir, cutangle, backangle):
                        d.warning = (("G71: The programmed profile has an entry ramp angle of %s and can not be cut "
                                       "with the active tool backangle of %s") % (cutangle,  backangle))


    #And now make the cuts. If there are any

    if len(cuts) == 0:
        d.warning = ("The combination of the start point, initial move and profile blocks "
                     "do not result in any machinable cuts being found")
        return
    
    lastcut = cuts[0]

    print "max pocket", d.pmax

    print  d.retracts

    for p in range(0, d.pmax + 1):
        pose[axes[P]] = d.retracts[p]
        emccanon.STRAIGHT_TRAVERSE(2, *pose)    
        for c in cuts:            
            #[0]pocket[1]x [2]ys [3]ye [4]entry [5]exit
            if c[0] == p:
                pose[axes[Q]] = c[4]
                emccanon.STRAIGHT_TRAVERSE(p, *pose)
                pose[axes[P]] = c[1] - d.Dword
                emccanon.STRAIGHT_TRAVERSE(p, *pose) 
                pose[axes[P]] = c[1]
                pose[axes[Q]] = c[2]
                emccanon.STRAIGHT_FEED(p, *pose)
                pose[axes[Q]] = c[3]
                emccanon.STRAIGHT_FEED(p, *pose)
                pose[axes[P]] = c[1] - d.Dword
                pose[axes[Q]] = c[5]
                emccanon.STRAIGHT_FEED(p, *pose)
                # pre-load the retract in case end of pocket
                #pose[axes[Q]] = c[2]
                lastcut = c
                
        emccanon.STRAIGHT_TRAVERSE(5, *pose)             
          
    pose[axes[P]] = d.x_begin
    emccanon.STRAIGHT_TRAVERSE(2, *pose)    
    pose[axes[Q]] = d.y_begin   
    emccanon.STRAIGHT_TRAVERSE(2, *pose)    
    return INTERP_OK

def g71(self, **words):
    g7x(self, 710, **words)
    return d.warning if d.warning != "" else INTERP_OK
    
def g711(self, **words):
    g7x(self, 711, **words)
    return d.warning if d.warning != "" else INTERP_OK

def g72(self, **words):
    g7x(self, 720, **words)
    return d.warning if d.warning != "" else INTERP_OK
    
def g721(self, **words):
    g7x(self, 721, **words)
    return d.warning if d.warning != "" else INTERP_OK

