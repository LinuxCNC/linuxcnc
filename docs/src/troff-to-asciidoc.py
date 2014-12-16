import os, sys, re

SourceFilePath = sys.argv[1]
TargetFilePath = sys.argv[2]

print "source  = " + SourceFilePath
print "target  = " + TargetFilePath

# open the source file
SourceLines = open(SourceFilePath, 'rU').readlines()

# new content
ComponentTitle = []
AsciidocLines = []
IndexLines = []
#if the first character is a point, then check further, else get the next line

#.TH Header
re_TH = re.compile(r'^\.TH')

#.TP Paragraph
re_TP = re.compile(r'^\.TP')

#.HP ?
re_HP = re.compile(r'^\.HP')

#.TQ Paragraph
re_TQ = re.compile(r'^\.TQ')

#.SH NAME
#.SH SYNOPSYS
re_SH = re.compile(r'^\.SH')

#.B
re_B = re.compile(r'^\.B')

#.de TQ
re_de_TQ = re.compile(r'^\.de TQ')

#.br
re_br = re.compile(r'^\.br')

#.ns
re_ns = re.compile(r'^\.ns')

#..
re_pointpoint = re.compile(r'^\.\.')

i = 0

while (i < len(SourceLines)):
    #TODO: parse the line for the markup
    #/fI until the next /fB or /fR means that until the next markup marker
    #
    result_re_TH = re_TH.search(SourceLines[i])
    if (result_re_TH != None):
        # the title must be split from the line, added, and underlined with the
        # same amount of '=' signs underneath
        CompTitle=SourceLines[i].split(' ')[1]
        ComponentTitle.append(CompTitle)
        ComponentTitle.append("="*len(CompTitle))
        #ComponentTitle.append('\n')
    #
    result_re_SH = re_SH.search(SourceLines[i])
    if (result_re_SH != None):
        #.SH has been found, get the name, put it in the index and add the line
        CompHeader=SourceLines[i].split(' ')[1]
        AsciidocLines.append('\n' + "===== " + CompHeader)
        IndexLines.append(CompHeader)
        #think about anchoring the index to the header
    #
    result_re_B = re_B.search(SourceLines[i])
    if (result_re_B != None):
        #read the line, remove the ".B " and add to the Asciidoclines
        CurrLine=SourceLines[i][3:len(SourceLines[i])]
        AsciidocLines.append(CurrLine)
    #
    #if we are in a paragraph, continue with filling and parsing lines until
    #the next paragraph is encountered
    #
    result_re_TP = re_TP.search(SourceLines[i])
    if (result_re_TP != None):
        #add empty line
        AsciidocLines.append('\n')
        #CurrLine=SourceLines[i][:len(SourceLines[i])]
    #
    #these lines should not do anything and are to be ignored
    result_re_de_TQ = re_de_TQ.search(SourceLines[i])
    result_br = re_br.search(SourceLines[i])
    result_HP = re_HP.search(SourceLines[i])
    result_TQ = re_TQ.search(SourceLines[i])
    result_ns = re_ns.search(SourceLines[i])
    result_pointpoint = re_pointpoint.search(SourceLines[i])
    if not ((result_re_de_TQ != None) \
            or (result_br != None) \
            or (result_HP != None) \
            or (result_TQ != None) \
            or (result_ns != None) \
            or (result_pointpoint != None)):
        #nothing to be done unless all other results are none
        #in that situation just copy the lines
        if (result_re_B == None) \
                and (result_re_SH == None) \
                and (result_re_TP == None) \
                and (result_re_TH == None):
            AsciidocLines.append(SourceLines[i])

    i += 1
#return to while loop

#now write all info into the target file
AsciidocFile = open(TargetFilePath, 'w+')

# starting with the component title
for prog_line in ComponentTitle:
    #    print line
    print >> AsciidocFile, prog_line

#the index
for prog_line in IndexLines:
    #    print line
    print >> AsciidocFile, prog_line

#the content
for prog_line in AsciidocLines:
    #    print line
    print >> AsciidocFile, prog_line
