import os, sys, re

GcodeFileInput = sys.argv[1]
print GcodeFileInput
GcodeFileOutput = "swapped-" + GcodeFileInput

# now the following occurs:
# the time of the retract period is not set before the disconnect of the
# extruder with the nozzle motion. It's something in the slic3r output
# before the .py script the file is like this:
#
#G1 X-15.574 Y-18.156 A541.86412 ; fill
#G0 Z9.000 F18000.000 ; move to next layer (44)
#G1 F1800.000 A540.56412 ; retract
#
#and afterwards it's like this:
#
#G1 X-15.574 Y-18.156 A541.86412 ; fill
#M65 P2
#G0 Z9.000 F18000.000 ; move to next layer (44)
#M68 E4 Q0.2

#define regular expressions to test for
# move to next layer
re_move_layer = re.compile(r'; move to next layer \(')
# move like infill
re_move_other = re.compile(r'^G[01]')
# disconnect with motion
re_disconnect = re.compile(r'^M65 P2')
# set retract time
re_retract_time= re.compile(r'^M68 E4 Q.*')

#fileDir = os.path.abspath(sys.argv[0])
#fileDir = os.path.dirname(fileDir)
#file_path_GcodeInput = os.path.join(fileDir, GcodeFileInput)
#GcodeLines = open(file_path_GcodeInput, 'rU').readlines()
GcodeLines = open(GcodeFileInput, 'rU').readlines()

# new program for output
GcodeProgram = []

# traverse G-code file
# for i in range(len(GcodeLines)):
i=0
while (i < len(GcodeLines)):
    result_disconnect = re_disconnect.search(GcodeLines[i])
    if i < len(GcodeLines)-1:
        result_move_next_layer = re_move_layer.search(GcodeLines[i+1])
        result_move_other = re_move_other.search(GcodeLines[i+1])
    if i < len(GcodeLines)-2:
        result_retract_time = re_retract_time.search(GcodeLines[i+2])
    if ((result_disconnect != None) \
        & ((result_move_next_layer != None) \
        | (result_move_other != None))
        & (result_retract_time != None)):
        # now write first the 3rd line, then the first, and lastly the second
        GcodeProgram.append(GcodeLines[i+2] + ";bla")
        GcodeProgram.append(GcodeLines[i])
        GcodeProgram.append(GcodeLines[i+1])
        i += 2
    else:
        GcodeProgram.append(GcodeLines[i])
    i += 1

#return to while loop


GcodeFile = open(GcodeFileOutput, 'w+')
for prog_line in GcodeProgram:
    #    print line
    print >> GcodeFile, prog_line




