#!/usr/bin/python

import bisect
import os
import sys
from machinekit import config


class R2Temp:
    def __init__(self, name):
        # Thermistor data is loaded at runtime from thermistor_tables folder
        self.thermistor_table_raw = []
        self.thermistor_table = []
        self.R_key = []

        self.loadTable(name)

        if len(self.thermistor_table_raw) == 0:
            sys.stderr.write('thermistor_table_raw table not found\n')
            exit(1)

        # Shuffle array to make three lists of values (Temp, Resistane, Alpha)
        # so we can use bisect to efficiently do table lookups
        self.thermistor_table = list(map(list, list(zip(*self.thermistor_table_raw))))

        # Pull out the resistance values to use as a key for bisect
        self.R_key = self.thermistor_table[1]

    def loadTable(self, name):
        c = config.Config()
        localInputFile = os.path.join(os.getcwd(), 'thermistor_tables', name + '.txt')
        systemInputFile = os.path.join(c.datadir, 'fdm', 'thermistor_tables', name + '.txt')
        
        if os.path.exists(localInputFile):
            inputFile = localInputFile
        elif os.path.exists(systemInputFile):
            inputFile = systemInputFile
        else:
            sys.stderr.write('Thermistor table was not found\n')
        
        with open(inputFile, "r") as f:
            self.thermistor_table_raw = []
            content = f.readlines()
            for line in content:
                line = ' '.join(line.split())
                if ((len(line) == 0) or (line[0] == '#')):
                    continue
                datas = line.split(' ')
                tableEntry = []
                for data in datas:
                    tableEntry.append(float(data))
                self.thermistor_table_raw.append(tableEntry)

        if len(self.thermistor_table_raw) == 0:
            return

        # Temperature table needs resistance to be ordered low [0] to high [n]
        if (self.thermistor_table_raw[0][0] < self.thermistor_table_raw[-1][0]):
            self.thermistor_table_raw.reverse()

    # Convert resistance value into temperature, using thermistor_table table
    def r2t(self, R_T):
        temp_slope = 0.0
        temp = 0.0

        i = max(bisect.bisect_right(self.R_key, R_T) - 1, 0)

        if (i != (len(self.thermistor_table[0]) - 1)):
            temp_slope = (self.thermistor_table[0][i] - self.thermistor_table[0][i + 1]) / (self.thermistor_table[1][i] - self.thermistor_table[1][i + 1])
        temp = self.thermistor_table[0][i] + ((R_T - self.thermistor_table[1][i]) * temp_slope)
        #print "Temp:", temp, "i.R_T:", i, R_T, "slope:", temp_slope,
        #print "Deg.left:", self.thermistor_table[1][i], "Deg.right:", self.thermistor_table[1][i+1]
        return temp

#r2temp = R2Temp("1")
#print((r2temp.r2t(5000)))
#r2temp2 = R2Temp("5")
#print((r2temp2.r2t(5000)))
#print((r2temp.r2t(5000)))