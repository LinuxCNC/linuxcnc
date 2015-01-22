#!/usr/bin/python
# -*- coding: utf-8 -*-
import os
import sys
import codecs
import argparse

replacements = [
    [" E", " A"],
    ["G10", "G22"],
    ["G11", "G23"]
    ]
unsupported = [
    "M82"
    ]
endCode = "M2"


def do_replacements(line):
    for replacement in replacements:
        line = line.replace(replacement[0], replacement[1])
    for code in unsupported:
        if code in line:
            line = ""
            break
    return line

def main():
    parser = argparse.ArgumentParser(description='This application converts RepRap flavour GCode to Machinekit flavour GCode')
    parser.add_argument('input', nargs=1, help='input file')
    parser.add_argument('output', nargs='?', help='output file, prints output to stdout of not specified', default=None)
    parser.add_argument('-d', '--debug', help='enable debug mode', action='store_true')

    args = parser.parse_args()

    hasProgramEnd = False
    inputFile = args.input[0]
    outputFile = args.output
    inFile = None
    outFile = None

    try:
        inFile = codecs.open(inputFile, "r", "utf-8")
    except IOError as e:
        sys.stdout.write(str(e) + '\n')
        exit(1)
        
    try:
        if outputFile is not None:
            outFile = codecs.open(outputFile, "w", "utf-8")
    except IOError as e:
        sys.stdout.write(str(e) + '\n')
        inFile.close()
        exit(1)

    for line in inFile:
        newline = do_replacements(line)
        if outFile is None:
            sys.stdout.write(newline)
        else:
            outFile.write(newline)
        if (newline[:2] == "M2") or (newline[:1] == "%"):  # check for end of program
            hasProgramEnd = True
            
    if not hasProgramEnd:
        if outFile is None:
            sys.stdout.write(endCode + "\n")
        else:
            outFile.write(endCode + "\n")
        
    inFile.close()
    if outFile is not None:
        outFile.close()
    exit(0)
    
if __name__ == "__main__":
    main()