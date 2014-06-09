#!/usr/bin/python

#    Copyright 2013 Frank Tkalcevic (frank@franksworkshop.com.au)
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

# Version 1.1
# 	- Fix problem not picking up F word

import fileinput
import sys
import re
import os
from author import douglas

plane=17
point_tolerance=0.05
length_tolerance=0.005

output_file = ""

def output_line( s ):
    if output_file == "":
        print s
    else:
        with open(output_file, "a") as myfile:
            myfile.write(s + "\n")

class gcode(object):
    def __init__(self):
        self.regMatch = {}
        self.line_count = 0
        self.output_line_count = 0


    def load(self, filename):
        if os.path.isfile(filename):
            self._fileSize = os.stat(filename).st_size
            gcodeFile = open(filename, 'r')
            self._load(gcodeFile)
            gcodeFile.close()


    def loadList(self, l):
        self._load(l)


    def _load(self, gcodeFile):

        st = []
        lastx = 0
        lasty = 0
        lastz = 0
        lasta = 0
        lastf = 0
        self.line_count = 0
        self.output_line_count = 0


        for line in gcodeFile:

            self.line_count = self.line_count + 1
            line = line.rstrip()
            original_line = line
            if type(line) is tuple:
                line = line[0]

            if ';' in line or '(' in line:
                sem_pos = line.find(';')
                par_pos = line.find('(')
                pos = sem_pos
                if pos is None:
                    pos = par_pos
                elif par_pos is not None:
                    if par_pos > sem_pos:
                        pos = par_pos
                comment = line[pos+1:].strip()
                line = line[0:pos]

            # we only try to simplify G1 coordinated moves
            G = self.getCodeInt(line, 'G')
            if G == 1:    #Move
                x = self.getCodeFloat(line, 'X')
                y = self.getCodeFloat(line, 'Y')
                z = self.getCodeFloat(line, 'Z')
                a = self.getCodeFloat(line, 'A')
                f = self.getCodeFloat(line, 'F')

                if x is None: 
                    x = lastx
                if y is None: 
                    y = lasty
                if z is None: 
                    z = lastz
                if a is None: 
                    a = lasta
                if f is None: 
                    f = lastf

                st.append( [x,y,z,a,f] )

                lastx = x
                lasty = y
                lastz = z
                lasta = a
                lastf = f
            else:
                # any other move signifies the end of a list of line segments,
                # so we simplify them.

                if G == 0:    #Rapid - remember position
                    x = self.getCodeFloat(line, 'X')
                    y = self.getCodeFloat(line, 'Y')
                    z = self.getCodeFloat(line, 'Z')
                    a = self.getCodeFloat(line, 'A')

                    if x is not None: 
                        lastx = x
                    if y is not None: 
                        lasty = y
                    if z is not None: 
                        lastz = z 
                    if a is not None: 
                        lasta = a

                if len(line) > 0 and len(st) > 0:
                    self.simplifyPath(st)
                    st = []
                output_line( original_line )
                self.output_line_count = self.output_line_count + 1

        if len(st) != 0:
            self.simplifyPath(st)

        output_line( "; GCode file processed by " + sys.argv[0] )
        output_line( "; Input Line Count = " + str(self.line_count) )
        output_line( "; Output Line Count = " + str(self.output_line_count) )
        if self.output_line_count < self.line_count:
            output_line( "; Line reduction = " + str(100 * (self.line_count - self.output_line_count)/self.line_count) + "%" )


    def getCodeInt(self, line, code):
        if code not in self.regMatch:
            self.regMatch[code] = re.compile(code + '([^\s]+)', flags=re.IGNORECASE)
        m = self.regMatch[code].search(line)
        if m == None:
            return None
        try:
            return int(m.group(1))
        except:
            return None


    def getCodeFloat(self, line, code):
        if code not in self.regMatch:
            self.regMatch[code] = re.compile(code + '([^\s]+)', flags=re.IGNORECASE)
        m = self.regMatch[code].search(line)
        if m == None:
            return None
        try:
            return float(m.group(1))
        except:
            return None

    def simplifyPath(self, st):
        #print "st=",len(st)
        l = douglas(st, plane=plane, tolerance=point_tolerance, length_tolerance=length_tolerance)
        #print l
        #print len(l)
        for i, (g, p, c) in enumerate(l):
            self.output_line_count = self.output_line_count + 1
            #print "i, g,p,c=", i, g,p,c
            s = g + " "
            if p[0] is not None:
                s = s + "X{0:f}".format(p[0]) + " "
            if p[1] is not None:
                s = s + "Y{0:f}".format(p[1]) + " "
            if p[2] is not None:
                s = s + "Z{0:f}".format(p[2]) + " "
            if p[3] is not None:
                s = s + "A{0:f}".format(p[3]) + " "
            if p[4] is not None:
                s = s + "F{0:f}".format(p[4]) + " "
            if c is not None:
                s = s + c
            s = s.rstrip()
            output_line( s )

# Check command line for file name
if len(sys.argv) > 1:
    # input is file name
    output_file = sys.argv[1]
    input_file = output_file + ".bak"
    if os.path.isfile(input_file):
        os.remove( input_file )
    os.rename( output_file, input_file )
    gcode().load( input_file )

else:
    # input is via stdin
    gcode().loadList(fileinput.input())

