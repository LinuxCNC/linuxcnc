import os, sys, re

GcodeFileInput = sys.argv[1]
w_perim = sys.argv[2]
w_infill = sys.argv[3]
h_layer = sys.argv[4]
h_first = sys.argv[5]

GcodeFileOutput = "result-" + GcodeFileInput

# in the .hal file are netted:
#
# net line-width  motion.analog-out-02 => mult2.0.in0
# net line-height motion.analog-out-03 => mult2.0.in1
#
# this means that with the M68 E.. Q.. the signals
# line-width and line-height are set from the G-code
cmd_line_width = "M68 E2 Q"
cmd_line_height = "M68 E3 Q"
# the retract and precharge are done with setting/resetting
# motion.digital-out-02. This couples the extrudion to the motion and
# on the rising edge triggers a precharge, on the falling edge a retract

# net trigger motion.digital-out-02 oneshot.0.in mux4.0.sel1
cmd_extr_connect = "M64 P2"
cmd_extr_disconnect = "M65 P2"

#should there be error checking on all input arguments?

#print "Gcodefile    = " + GcodeFileInput
print "perimeter w  = " + w_perim
print "infill    w  = " + w_infill
print "layer height = " + h_layer
print "first layer  = " + h_first

#define regular expressions to test for

# test of there is an Ax.xxxx or A-x.xxxx in the string
re_extr_a_position = re.compile(r' A[0-9]+\-*\.[0-9]+ ')
# test for G1 at the beginning of a line
re_cmd_G1 = re.compile(r'^G1')
# first layer start
re_hgt_first_layer = re.compile(r'; move to next layer \(0\)')
# other layer start
re_hgt_other_layer = re.compile(r'; move to next layer \(1\)')
# perimeter
re_perimeter = re.compile(r'; perimeter')
# infill (furrently all infill is the same
re_fill = re.compile(r'; fill')
#re_w_inf_solid =
#re_w_inf_top =
# first layer start
re_skirt = re.compile(r'; skirt')
#find M68 retrach/precharge in line
re_retract_precharge = re.compile(r'^M68 E4 Q.*')

#fileDir = os.path.abspath(sys.argv[0])
#fileDir = os.path.dirname(fileDir)
#file_path_GcodeInput = os.path.join(fileDir, GcodeFileInput)
#GcodeLines = open(file_path_GcodeInput, 'rU').readlines()
GcodeLines = open(GcodeFileInput, 'rU').readlines()

# new program for output
GcodeProgram = []
b_firstlayer = False
b_extr_disconnect = False
b_extr_connect = False
b_linetypechange_up = False
b_otherlayer = False
b_linetype_infill = False
b_linetype_perimeter = False
prevous_linetype = ""
current_linetype = ""
prevous_layer = ""
current_layer = ""
# traverse G-code file
#for i in range(len(GcodeLines)):
i=0
while (i < len(GcodeLines)):
    # set line to ""
    line = ""
    # do some checks
    result_A_curr = re_extr_a_position.search(GcodeLines[i])
    result_G1_curr = re_cmd_G1.search(GcodeLines[i])
    result_firstlayer = re_hgt_first_layer.search(GcodeLines[i])
    result_retract_precharge_curr = re_retract_precharge.search(GcodeLines[i])
    # only do below when there is a line to read (end of file)
    if i < len(GcodeLines)-1:
        result_A_next = re_extr_a_position.search(GcodeLines[i+1])
        result_G1_next = re_cmd_G1.search(GcodeLines[i+1])
        result_otherlayer = re_hgt_other_layer.search(GcodeLines[i+1])
        result_perimeter = re_perimeter.search(GcodeLines[i+1])
        result_infill = re_fill.search(GcodeLines[i+1])
        result_skirt = re_skirt.search(GcodeLines[i+1])
        result_retract_precharge_next = re_retract_precharge.search(GcodeLines[i+1])

    if i > 1:
        result_A_prev = re_extr_a_position.search(GcodeLines[i-1])
    # decide upon results above
    if result_A_curr != None:
        #line += "A is there\n"
        #str_split = re_extr_a_position.split(GcodeLines[i])
        #new_line = str_split[1]+str_split[2]
        if result_A_next == None:
        #   disconnect (and retract) the extruder velocity with the nozzle
        #   velocity because extruder position not on next line
        #   line += "add disconnect code\n"
            b_extr_disconnect = True
        #   connect (and precharge) the extruder velocity with the nozzle
        #   velocity after this line because extruder position isnt previous line
        #   line += "add connect code\n"
    else:
        #   there is no A position, now we need to look for
        #   the next line to have an A position
            if result_A_next != None:
        #   line += "add precharge code\n"
                b_extr_connect = True
    
    if result_firstlayer != None:
        #line += "first layer\n"
        #set the layer height with G-code
        #line += "add Gcode for setting first layer height\n"
        b_firstlayer = True
        current_layer = "first layer"

    if result_otherlayer != None:
        if current_layer == "first layer":
            # search for when we leave first layer
            b_otherlayer = True
            current_layer = "other layer"

    if result_skirt != None:
        if prevous_linetype != "perimeter":
            b_linetypechange_up = True
            b_linetype_perimeter = True
            #prevous_linetype = current_linetype
            current_linetype = "perimeter"
    if result_perimeter != None:
        if prevous_linetype != "perimeter":
            b_linetypechange_up = True
            b_linetype_perimeter = True
            #prevous_linetype = current_linetype
            current_linetype = "perimeter"
    if result_infill != None:
        if prevous_linetype != "infill":
            b_linetypechange_up = True
            b_linetype_infill = True
            #prevous_linetype = current_linetype
            current_linetype = "infill"
    if result_retract_precharge_curr != None:
        b_retract_precharge_curr = True
    else:
        b_retract_precharge_curr = False
    if result_retract_precharge_next != None:
        b_retract_precharge_next = True
    else:
        b_retract_precharge_next = False

# wait and make sure that the connecting/disconnecting of the
# velocity with the nozzle movement is done after setting
# the time of the retract. So skip one line before adding
# the said g-code

# specific order for setting line width and/or height in
# combination with connecting extruder velocity with the
# motion (and thus) precharge
# first do the precharge
# then change line widths and heiht

    if b_linetypechange_up == True:
        prevous_linetype = current_linetype
        b_linetypechange_up = False
        if b_linetype_perimeter == True:
            #line += "** change line width to perimeter code\n"
            line += cmd_line_width + w_perim + "\n"
            #GcodeProgram.append(GcodeLines[i]+line)
            b_linetype_perimeter = False
        if b_linetype_infill == True:
            #line += "** change line width to infill code\n"
            line += cmd_line_width + w_infill + "\n"
            #GcodeProgram.append(GcodeLines[i]+line)
            b_linetype_infill = False

    if b_firstlayer == True:
        #line += "** first layer height code\n"
        line += cmd_line_height + h_first + "\n"
        #GcodeProgram.append(GcodeLines[i]+line)
        b_firstlayer = False
    if b_otherlayer == True:
        #line += "** other layer height code\n"
        line += cmd_line_height + h_layer + "\n"
        #GcodeProgram.append(GcodeLines[i]+line)
        b_otherlayer = False
    if b_extr_disconnect == True:
        if b_retract_precharge_next != True:
            line += cmd_extr_disconnect + "\n"
            b_extr_disconnect = False
    
    if b_extr_connect == True:
        line += cmd_extr_connect + "\n"
        if b_retract_precharge_curr != True:
            #line += "** precharge code\n"
            #line += cmd_extr_connect + "\n"
            b_extr_connect = False

    if (((b_extr_disconnect == True) & \
         (b_retract_precharge_curr == True)) | \
        ((b_extr_connect == True) & \
         (b_retract_precharge_curr == True))):
        GcodeProgram.append(line + GcodeLines[i])
        b_extr_disconnect = False
        b_extr_connect = False
    else:
        GcodeProgram.append(GcodeLines[i]+line)

    i += 1
#return to while loop


GcodeFile = open(GcodeFileOutput, 'w+')
for prog_line in GcodeProgram:
    #    print line
    print >> GcodeFile, prog_line




