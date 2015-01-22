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
re_SH = re.compile(r'^\.SH\s+(.+)\n')
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
#markup
re_fI = re.compile(r'\\fI')
re_fB = re.compile(r'\\fB')
re_fR = re.compile(r'\\fR')
re_longdash = re.compile(r'\\-')
re_erroneous_markup = re.compile( ('\*\*\*\*|\_\_\_\_') )
#.\" Comments
re_comment = re.compile(r'^\.\\"')

i = 0
while (i < len(SourceLines)):
    #/fI until the next /fB or /fR means that until the next markup marker
    #
    result_re_fI = re_fI.search(SourceLines[i])
    result_re_fB = re_fB.search(SourceLines[i])
    result_re_fR = re_fR.search(SourceLines[i])
    result_re_longdash = re_longdash.search(SourceLines[i])
    #first time for gettin into while loop
    CurrLine = SourceLines[i]
    str_asciiline = ""
    int_Character = 0
    last_formatting_char = ''
    #
    while ((result_re_fI != None) \
            or (result_re_fB != None) \
            or (result_re_fR != None)):
        #parse CurrLine
        #search for the first delimiter \f with next character 'I', 'B' or 'R'
        #when the first characters are found. The part of the string before will
        #go to the str_asciiline, the last_formatting_char will be remembered
        #because in asciidoc italic needs to be closed like this _italic_
        int_Character = CurrLine.find("\\f")
        #tempchar1 = CurrLine[int_Character+1]
        #tempchar2 = CurrLine[int_Character+2]
        if (int_Character != -1) :
            if (CurrLine[int_Character+2]=='I'):
                str_asciiline = str_asciiline + \
                                CurrLine[0:int_Character] + \
                                last_formatting_char + \
                                "__"
                last_formatting_char = "__"
            if (CurrLine[int_Character+2]=='B'):
                str_asciiline = str_asciiline + \
                                CurrLine[0:int_Character] + \
                                last_formatting_char + \
                                "**"
                last_formatting_char = "**"
            if (CurrLine[int_Character+2]=='R'):
                str_asciiline = str_asciiline + \
                                CurrLine[0:int_Character] + \
                                last_formatting_char + \
                                ""
                last_formatting_char = ""
            #
        CurrLine = CurrLine[int_Character+3:len(CurrLine)]
        #check for more
        result_re_fI = re_fI.search(CurrLine)
        result_re_fB = re_fB.search(CurrLine)
        result_re_fR = re_fR.search(CurrLine)
        #
        if ((result_re_fI == None) \
            and (result_re_fB == None) \
            and (result_re_fR == None)):
            #exiting the while loop, the SourceLines[1] now must contain the
            #cleaned version of the sentence for further processing
            str_asciiline += CurrLine + last_formatting_char
            #str_asciiline.append(last_formatting_char)
            SourceLines[i] = str_asciiline
    #
    #check for **** or ____ in str_asciiline and remove these
    #this happens for example when the sourcefile has \\fB\\fB in the
    #line. This will result in **** and renders faulty in asciidoc
    result_re_erroneous_markup = re_erroneous_markup.search(SourceLines[i])
    if (result_re_erroneous_markup != None):
        SourceLines[i] = re_erroneous_markup.sub('',SourceLines[i])
    if (result_re_longdash != None):
        #SourceLines[i].replace("\\-","--")
        CurrLine = re_longdash.sub("--",SourceLines[i])
        SourceLines[i] = CurrLine
    #done markup
    #
    result_re_TH = re_TH.search(SourceLines[i])
    if (result_re_TH != None):
        # the title must be split from the line, added, and underlined with the
        # same amount of '=' signs underneath
        CompTitle=SourceLines[i].split(' ')[1]
        ComponentTitle.append(CompTitle+'\n')
        ComponentTitle.append("="*len(CompTitle)+'\n\n')
        #ComponentTitle.append('\n')
    #
    result_re_SH = re_SH.search(SourceLines[i])
    if (result_re_SH != None):
        #.SH has been found, get the name, put it in the index and add the line
        CompHeader=result_re_SH.groups()[0]
        #if the header is between quotes, like: "see also" then strip the
        #quotes and change the space to a dash. in the indexlines array
        CompHeader = CompHeader.strip('\"')
        CompHeaderDashed = CompHeader.replace(" ", "-")
        #result_re_between_quotes = re_between_quotes.search(CompHeader)
        AsciidocLines.append("\n\n" + "===== " + \
                             "[[" + CompHeaderDashed.lower() + "]]" \
                             + CompHeader + "\n")
        IndexLines.append(". <<" + CompHeaderDashed.lower() + "," + \
                          CompHeader + ">>" + "\n")
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
    #
    #these lines should not do anything and are to be ignored
    result_re_comment = re_comment.search(SourceLines[i])
    result_re_de_TQ = re_de_TQ.search(SourceLines[i])
    result_br = re_br.search(SourceLines[i])
    result_HP = re_HP.search(SourceLines[i])
    result_TQ = re_TQ.search(SourceLines[i])
    result_ns = re_ns.search(SourceLines[i])
    result_pointpoint = re_pointpoint.search(SourceLines[i])
    if not ((result_re_comment != None) \
            or (result_re_de_TQ != None) \
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

AsciidocFile.writelines(ComponentTitle)
AsciidocFile.writelines(IndexLines)
AsciidocFile.writelines(AsciidocLines)

AsciidocFile.close()
