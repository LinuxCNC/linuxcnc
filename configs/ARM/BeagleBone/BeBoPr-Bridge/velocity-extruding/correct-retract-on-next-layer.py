import os, sys, re

GcodeFileInput = sys.argv[1]
#print GcodeFileInput
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
#
#
#but also multiple lines in between like
#M65 P2
#G0 X17.755 Y-14.143 F18000.000 ; move inwards before travel
#G0 Z0.600 F18000.000 ; move to next layer (2)
#M68 E4 Q0.2

#define regular expressions to test for
# move to next layer
re_move_layer = re.compile(r'; move to next layer \(')
# move like infill
re_move_other = re.compile(r'^G[01]')
# disconnect with motion
re_disconnect = re.compile(r'^M65 P2')
# connect with motion
re_connect = re.compile(r'^M64 P2')
# set retract time
re_retract_time= re.compile(r'^M68 E4 Q\.*')

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
j=0
while (i < len(GcodeLines)):
    #re_disconnect is a start of a sub loop, say 1 = 543
    #search from this line in a loop for re_retract_time.
    #if no re_retract_time then put the offset line in a buffer
    #if re_retract time is found, than that line needs to be first before the
    #current line. remember the line offset. like offset = 3, meaning in
    #line 546 the re_retract_time is found.
    #if re_connect occurs the buffer must be written to the new file because
    #there is no retract happening in between.
    result_disconnect = re_disconnect.search(GcodeLines[i])
    if (result_disconnect != None):
        b_exit = False
        j=1
        buffer = []
        while (b_exit == False):
            if ((i + j) < len(GcodeLines)):
                result_connect = re_connect.search(GcodeLines[i+j])
                result_retract_time = re_retract_time.search(GcodeLines[i+j])
                # if one of these below, then almost end of the sub loop
                if ((result_connect != None) \
                    | (result_retract_time != None)):
                    b_exit = True
                    # when terminated by retractino time, move retraction time
                    # the retract line (Gcodeline[i]) and don't add the current line
                    # to the buffer
                    if (result_retract_time != None):
                        GcodeProgram.append(GcodeLines[i+j])
                        GcodeProgram.append(GcodeLines[i])
                        for index, line in enumerate(buffer):
                            GcodeProgram.append(line)
                    else:
                        GcodeProgram.append(GcodeLines[i])
                        for index, line in enumerate(buffer):
                            GcodeProgram.append(line)
                        GcodeProgram.append(GcodeLines[i+j])
                    i += j
                # just a common line so append the current line to the buffer
                else:
                    buffer.append(GcodeLines[i+j])

            # end of file is reached, no connect to be found
            else:
                b_exit = True
                GcodeProgram.append(GcodeLines[i])
                for index, line in enumerate(buffer):
                    GcodeProgram.append(line)
                #GcodeProgram.append(GcodeLines[i+j])
                i += j
            # up number for next line in loop
            j += 1
    else:
        GcodeProgram.append(GcodeLines[i])
    i += 1

#return to while loop

GcodeFile = open(GcodeFileOutput, 'w+')
for prog_line in GcodeProgram:
    #    print line
    print >> GcodeFile, prog_line




