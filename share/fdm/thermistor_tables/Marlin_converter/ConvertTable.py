#!/usr/bin/python
# encoding: utf-8
"""
ConvertTable.py

Created by Alexander Rössler on 2014-03-24.
"""

import argparse
import string


def v2R(value):
    return (4700.0*value)/(1024.0-value)

parser = argparse.ArgumentParser(description='Converts a Marlin thermistor table to Machinekit thermistor table')
parser.add_argument('-i', '--input', help='input file', required=True)
parser.add_argument('-o', '--output', help='output file', required=True)

args = parser.parse_args()

inputFile = args.input
outputFile = args.output

with open(inputFile, "r") as f:
    with open(outputFile, "w") as out:
        out.write("# Autoconverted thermistor table for " + inputFile + "\n")
        out.write("# Temp\tResistance\taplha\n")
        content = f.readlines()
        for line in content:
            line = ''.join(line.split())
            line = string.replace(line, '{', '')
            line = string.replace(line, '}', '')
            items = line.split(',')
            resistance = v2R(float(items[0].split('*')[0]))
            temperature = float(items[1])
            alpha = 0.0
            out.write(str(temperature) + "\t" + '{0:.02f}'.format(resistance) + "\t" + str(alpha) + "\n")