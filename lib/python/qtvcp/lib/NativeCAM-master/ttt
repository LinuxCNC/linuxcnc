#!/usr/bin/env python
# coding: utf-8

import sys
import re
import getopt
import subprocess

def usage():
    print("""
Command :
   ttt.py [Options]

Options :
   -? | --help      this text
   -v               v_align = 0   Top=2:Center=1:Bottom=0
   -h               h_align = 0   Left=0:Center=1:Right=2
   -H               text_height   default 0.5
   -f               font          (not the file but 'Sans Serif', etc)
   -i               line_spacing  default 1
   -m               mode = 0      Normal=0:Mirrored=1
   -e               filled        Not=0:Filled=1
   -l               fill_scale    >=24
   -t               stretch       default 1.0
   -u               unicode       default 0
   -T               text          default 'NativeCAM' (can be multiline with /n)
   -n               name of sub   name defined by calling subroutine
""")

# default values
v_align = '0'  # v Bottom first line=0:Top of first line=1:Center=3:Bottom of last line=3
h_align = '0'  # h Left=0:Center=1:Right=2
text_height = 0.5 #H
font_file = '/usr/share/fonts/truetype/freefont/FreeSerifBoldItalic.ttf' #f
line_spacing = 1.0 #i
x_factor = 1  # m if Mirrored=1 x_factor = -1
filled = False  # e
fill_scale = '24' #l
stretch = 1.0  # t
ucode = ''  # u
text = 'NativeCAM'  # T
sub_name = 'ttt_def_sub'  # n

# read and validate options
try :
    optlist, arg = getopt.getopt(sys.argv[1:], 'h:v:H:f:i:m:e:l:t:n:T:?:u:', ['help'])
    optlist = dict(optlist)
except getopt.GetoptError as err:
    sys.exit(err)

if "-?" in optlist or '--help' in optlist:
    usage()
    print('')
    sys.exit(0)
if "-v" in optlist :
    v_align = optlist["-v"]
if "-h" in optlist :
    h_align = optlist["-h"]
if "-H" in optlist :
    text_height = float(optlist["-H"])
if "-f" in optlist :
    font_file = optlist['-f']
if "-i" in optlist :
    line_spacing = float(optlist["-i"])
if "-m" in optlist and optlist["-m"] == '1' :  # Mirrored
    x_factor = -1
if "-e" in optlist :
    filled = optlist["-e"] == '1'
if "-l" in optlist :
    fill_scale = optlist["-l"]
if "-t" in optlist :
    stretch = float(optlist["-t"]) / 100
if "-u" in optlist and optlist["-u"] == '1' :
    ucode = '-u'
if "-T" in optlist :
    t = optlist["-T"].lstrip('0')
    if t > '' :
        text = t
if "-n" in optlist and (optlist["-n"] > '') :
    sub_name = optlist["-n"]

cmd_str = ('truetype-tracer -s 100 %s -f "%s" ' % (ucode, font_file)) + '"%s"'

# find text standard height
s = cmd_str % 'ABCDEFGHKLMNOPRSTUVWXYZ'
try :
    gc_line = subprocess.check_output([s], shell = True, stderr = subprocess.STDOUT)
except subprocess.CalledProcessError as e:
    msg = 'Error with subprocess: returncode = %(errcode)s\n' \
             'output = %(output)s\ne= %(e)s\n' \
             % {'errcode':e.returncode, 'output':e.output, 'e':e}
    print('o<%s_engrave> sub\n' % sub_name)
    print('o<%s_engrave> endsub\n' % sub_name)
    exit(-1)

gc_line = re.sub(r"G00 Z #1\nM02\n", '', gc_line)
ending = gc_line[-15:]
Y_extend = int(ending[ending.find(' to ') + 3:ending.find(')')])
Y_scale = text_height / Y_extend

line_count = 0
for aline in text.split('\n') :
    line_count += 1

line_height = text_height * (1 + line_spacing)

if v_align == '0' :  # Bottom of first line
    Y_start = 0.0
elif v_align == '1' :  # Top of first line
    Y_start = - text_height
elif v_align == '2' : # center
    Y_start = line_height * (line_count - 1) / 2 - text_height / 2
else :  # bottom of last line
    Y_start = line_height * (line_count - 1)

result = ''
result_call = 'o<%s_engrave> sub\n' % sub_name

line_processed = 0
if filled :
    cmd_str = ('truetype-tracer -s 100 %s -l %s -f "%s" ' % (ucode, fill_scale, font_file)) + '"%s"'

for aline in text.split('\n') :
    gc_line = subprocess.check_output([cmd_str % aline], shell = True, stderr = subprocess.STDOUT)
    gc_line = re.sub(r"G00 Z #1\nM02\n", '', gc_line)

    ending = gc_line[-50:]
    X_extend = int(ending[ending.find('to ') + 3:ending.find(',')])

    gc_line = gc_line.replace("F#4", "")
#    gc_line = gc_line.replace(" (moveto)", "")
    gc_line = gc_line.replace(" (lineto)", "")
    gc_line = gc_line.replace("[0-#2]", "#2")
    gc_line = gc_line.replace("X [", " X[")
    gc_line = gc_line.replace("Y ", "Y")
    gc_line = gc_line.replace("Z ", " Z")
    gc_line = gc_line.replace("#3+#5", "#4+#5")  # 're' did not work in this case
    gc_line = gc_line.replace("#3] J", "#4] J")
    gc_line = gc_line.replace("] I[", "]  I[")
    gc_line = gc_line.replace("] J[", "]  J[")
    gc_line = gc_line.replace("*", " * ")
    gc_line = gc_line.replace("+", " + ")
    gc_line = gc_line.replace("/G", "G")
    gc_line = gc_line.replace("G01  Z#2", "G00  Z#<start>\nG01  Z#2 F#<plunge_feed>\nF#<normal_feed>")

    new_gc = ''
    for a_line in gc_line.split('\n') :
        if a_line.find('#1=') == 0 :
            new_gc += '\t#<start> = #3\n\t#<plunge_feed> = #4\n\t#<normal_feed> = #5\n'
        elif a_line.find('#2=') == 0 :
            continue
        elif a_line.find('#3=') == 0 :
            new_gc += '\t#3 = %f  (Y Scale)\n\t#4 = %f  (X Scale)\n' % (Y_scale, Y_scale * stretch * x_factor)
        elif a_line.find('#4=') == 0 :
            continue

        elif a_line.find('#5=') == 0 :
            if h_align == '0' :  # Left
                X_offset = 0.0
            elif h_align == '1' :  # Center
                X_offset = (-X_extend / 2 * Y_scale * stretch * x_factor)
            else : # Right
                X_offset = (-X_extend * Y_scale * stretch * x_factor)
            new_gc += '\t#5 = %f  (X offset)\n' % X_offset

        elif a_line.find('#6=') == 0 :
            new_gc += '\t#6 = %f  (Y offset)\n' % (Y_start - line_height * line_processed)

        elif a_line.find('(start of symbol') == 0 :
            new_gc += '\n\t%s\n' % a_line
        elif a_line.find('(overall extents:') == 0 :
            new_gc += '\t%s\n\n\tG00  Z#1\n' % a_line
        else :
            new_gc += '\t%s\n' % a_line

    result += 'o<%s-line-%d> sub\n%s' % (sub_name, line_processed, new_gc)
    result += 'o<%s-line-%d> endsub\n\n' % (sub_name, line_processed)
    result_call += '\to<%s-line-%d> CALL [#1] [#2] [#3] [#4] [#5]\n' % (sub_name, line_processed)
    line_processed += 1

print result + result_call + 'o<%s_engrave> endsub\n' % sub_name
