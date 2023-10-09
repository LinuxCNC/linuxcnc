#!/usr/bin/env python3
# Copyright (c) 2023 Jim Sloot (persei802@gmail.com)
# Originally created in java by Shawn E. Gano, shawn@ganotechnologies.com
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import sys
import os
import time
import math
import zmq

from PyQt5.QtCore import QObject


class GCode_Coordinate:
    MIN_Z_RADIUS = 0.25  # units (this could be .25in or .25mm [this case is very fast] - in the future may want to take into account metric to help safeguard this better)

    def __init__(self, parent, lineParts=None):
        self.parent = parent
        self.X = 0
        self.Y = 0
        self.Z = 0
        self.A = 0
        self.I = 0
        self.J = 0
        self.K = 0
        self.Xset = False
        self.Yset = False
        self.Zset = False
        self.Aset = False
        self.Iset = False
        self.Jset = False
        self.Kset = False
        if lineParts is not None: self.create_coords(lineParts)

    def create_coords(self, lineParts):
        for part in lineParts:
            if part.startswith("X"):
                value = part[1:]
                self.setX(float(value))
            elif part.startswith("Y"):
                value = part[1:]
                self.setY(float(value))
            elif part.startswith("Z"):
                value = part[1:]
                self.setZ(float(value))
            elif part.startswith("A"):
                value = part[1:]
                self.setA(float(value))
            elif part.startswith("I"):
                value = part[1:]
                self.setI(float(value))
            elif part.startswith("J"):
                value = part[1:]
                self.setJ(float(value))
            elif part.startswith("K"):
                value = part[1:]
                self.setK(float(value))

    def copy_unset_from_previous(self, previous_coord):
        if not self.isXset() and previous_coord.isXset():
            self.setX(previous_coord.getX())
        if not self.isYset() and previous_coord.isYset():
            self.setY(previous_coord.getY())
        if not self.isZset() and previous_coord.isZset():
            self.setZ(previous_coord.getZ())
        if not self.isAset() and previous_coord.isAset():
            self.setA(previous_coord.getA())

    def straight_distance(self, previous_coord, z0_offset, dist_units, error_if_not_set, line_number):
        square_sum_dist = 0
        if self.Xset and previous_coord.Xset:
            square_sum_dist += (self.X - previous_coord.X) ** 2
        elif self.Xset != previous_coord.Xset and error_if_not_set:
            self.parent.publish(f"G01 movement without an initial X starting point. (line: {line_number})")
        if self.Yset and previous_coord.Yset:
            square_sum_dist += (self.Y - previous_coord.Y) ** 2
        elif self.Yset != previous_coord.Yset and error_if_not_set:
            self.parent.publish(f"G01 movement without an initial Y starting point. (line: {line_number})")
        if self.Zset and previous_coord.Zset:
            square_sum_dist += (self.Z - previous_coord.Z) ** 2
        elif self.Zset != previous_coord.Zset and error_if_not_set:
            self.parent.publish(f"G01 movement without an initial Z starting point. (line: {line_number})")
        if self.Aset and previous_coord.Aset:
            if not self.Zset and not previous_coord.Zset:
                self.parent.publish(f"A-axis movement requires a prior Z-axis movement (for distance calculation). (line: {line_number})")

            max_z = max(abs(self.Z + z0_offset), abs(previous_coord.Z + z0_offset))
            tolerance_scale = 1.0
            if dist_units == "mm":
                tolerance_scale = 25.4

            if max_z < (self.MIN_Z_RADIUS * tolerance_scale):
                max_z = self.MIN_Z_RADIUS * tolerance_scale

            adiff = abs(self.A - previous_coord.A)
            arclength = max_z * (adiff * math.pi / 180.0)
            square_sum_dist += arclength ** 2

        return math.sqrt(square_sum_dist)

    def arc_distance_from_previous_coordinate(self, previous_coord, clockwise_direction, dist_units, line_number, current_plane_selected):
        if current_plane_selected == 17:
            if not previous_coord.Xset or not previous_coord.Yset:
                self.parent.publish(f"G02/G03 movement requires both X and Y to have been previously set (G17). (line: {line_number})")
            if not self.Xset or not self.Yset or not self.Iset or not self.Jset:
                self.parent.publish(f"G02/G03 movement requires X, Y, I, J arguments (G17). One or more are missing. (line: {line_number})")
        elif current_plane_selected == 18:
            if not previous_coord.Xset or not previous_coord.Zset:
                self.parent.publish(f"G02/G03 movement requires both Z and X to have been previously set (G18). (line: {line_number})")
            if not self.Xset or not self.Zset or not self.Iset or not self.Kset:
                self.parent.publish(f"G02/G03 movement requires X, Z, I, K arguments (G18). One or more are missing. (line: {line_number})")
        elif current_plane_selected == 19:
            if not previous_coord.Yset or not previous_coord.Zset:
                self.parent.publish(f"G02/G03 movement requires both Y and Z to have been previously set (G19). (line: {line_number})")
            if not self.Yset or not self.Zset or not self.Jset or not self.Kset:
                self.parent.publish(f"G02/G03 movement requires Y, Z, J, K arguments (G19). One or more are missing. (line: {line_number})")

        if current_plane_selected == 17:
            prev_x = previous_coord.X
            prev_y = previous_coord.Y
            curr_x = self.X
            curr_y = self.Y
            curr_i = self.I
            curr_j = self.J
        elif current_plane_selected == 18:
            prev_x = previous_coord.X
            prev_y = previous_coord.Z
            curr_x = self.X
            curr_y = self.Z
            curr_i = self.I
            curr_j = self.K
        elif current_plane_selected == 19:
            prev_x = previous_coord.Y
            prev_y = previous_coord.Z
            curr_x = self.Y
            curr_y = self.Z
            curr_i = self.J
            curr_j = self.K

        return self.arc_length(prev_x, prev_y, curr_x, curr_y, curr_i, curr_j, clockwise_direction, dist_units, line_number)

    def arc_length(self, prev_x, prev_y, curr_x, curr_y, curr_i, curr_j, clockwise_direction, dist_units, line_number):
        center_point_x = prev_x + curr_i
        center_point_y = prev_y + curr_j
        radius = math.sqrt(curr_i ** 2 + curr_j ** 2)

        r_error = abs(math.sqrt((curr_x - center_point_x) ** 2 + (curr_y - center_point_y) ** 2) - radius)
        tolerance_scale = 1.0
        if dist_units == "mm":
            tolerance_scale = 25.4

        if r_error > (0.001 * tolerance_scale):
            self.publish(f"G02/G03 movement is not circular. (line {line_number})")

        xx_1 = prev_x - center_point_x
        xx_2 = prev_y - center_point_y

        yy_1 = curr_x - center_point_x
        yy_2 = curr_y - center_point_y

        zz_1 = xx_1 * yy_1 + xx_2 * yy_2
        zz_2 = xx_1 * yy_2 - xx_2 * yy_1
        
        theta = math.atan2(zz_2, zz_1)
        if theta < 0:
            theta += 2.0 * math.pi
        if theta >= 2.0 * math.pi:
            theta -= 2.0 * math.pi

        clockwiseArcLength = radius * (2.0 * math.pi if theta == 0 else theta)
        counterClockwiseArcLength = radius * (2.0 * math.pi if theta >= 2.0 * math.pi else theta)

        if clockwise_direction is True:
            return clockwiseArcLength
        return counterClockwiseArcLength

    def rotationAbsoluteDifferenceDegrees(self, previousCoord):
        if self.isAset() is False or previousCoord.isAset() is False:
            return 0
        return abs(self.getA() - previousCoord.getA())
        
    def getX(self):
        return self.X

    def getY(self):
        return self.Y

    def getZ(self):
        return self.Z

    def getA(self):
        return self.A

    def getI(self):
        return self.I

    def getJ(self):
        return self.J

    def getK(self):
        return self.K

    def setX(self, x):
        self.X = x
        self.Xset = True

    def setY(self, y):
        self.Y = y
        self.Yset = True

    def setZ(self, z):
        self.Z = z
        self.Zset = True

    def setA(self, a):
        self.A = a
        self.Aset = True

    def setI(self, i):
        self.I = i
        self.Iset = True

    def setJ(self, j):
        self.J = j
        self.Jset = True

    def setK(self, k):
        self.K = k
        self.Kset = True

    def isXset(self):
        return self.Xset

    def isYset(self):
        return self.Yset

    def isZset(self):
        return self.Zset

    def isAset(self):
        return self.Aset

    def isIset(self):
        return self.Iset

    def isJset(self):
        return self.Jset

    def isKset(self):
        return self.Kset


class GCodeMovementResult:
    def __init__(self, is_movement_line, is_implicit_movement, gcode_string):
        self.is_movement_line = is_movement_line
        self.is_implicit_movement = is_implicit_movement
        
        if self.is_movement_line is True and self.is_implicit_movement is False:
            self.set_gcode(gcode_string)
        else:
            self.gcode_movement_type = -1  # not valid

    def set_gcode(self, gcode_string):
        if len(gcode_string) < 2:
            self.gcode_movement_type = -1  # not valid
        else:
            gcode_str = gcode_string[1:]
            self.gcode_movement_type = int(gcode_str)


class File_Converter(QObject):
    def __init__(self, infile, outfile, offset, units, mode):
        QObject.__init__(self)
        self.inputFilePath = infile
        self.outputFilePath = outfile
        self.z0_offset = float(offset)
        self.distUnits = units
        self.errorMessage = ""
        self.errorProcessingFile = False
        self.linesProcessed = 0
        self.endOfProgramFound = False
        self.start_percent_found = False
        self.lastCoordinate = GCode_Coordinate(self)
        self.currentFeedRate = -1
        self.lastGCode = -1
        self.G17_18_19_Found = False
        self.G90Found = False
        self.G93Found = False
        self.G94Found = False
        self.currentPlaneSelected = 17
        self.totalToolPathDistance = 0
        self.totalDegreesRotated = 0
        self.totalG0123Lines = 0
        self.totalG00lines = 0
        self.totalG01lines = 0
        self.totalG02lines = 0
        self.totalG03lines = 0
        self.rotaryMinMaxFound = False
        self.rotaryMax = 0
        self.rotaryMin = 0
        self.numberLinesWithRotaryMoves = 0
        self.fOutputPrecision = 3
        self.wrap_all = True if mode == '1' else False
        self.startTime = 0
        self.last_progress = 0
        self.start_process()
        # close the zmq connection
        self.socket.close()
        self.context.term()

    def start_process(self):
        # set up zero message queue as client
        self.context = zmq.Context()
        self.socket = self.context.socket(zmq.REQ)
        self.socket.connect('tcp://localhost:4096')
        
        self.clear_all_data()
        self.startTime = time.time()

        totalLinesInFile = self.countLinesInFile(self.inputFilePath)
        self.publish(f"Total lines in input file is {totalLinesInFile}")

        try:
            out_file = open(self.outputFilePath, 'w')
            in_file = open(self.inputFilePath, 'r')
            nextLine = in_file.readline()
            self.linesProcessed = 0
            while nextLine:
                line2Write = self.processInputLine(nextLine) + '\n'
                out_file.write(line2Write)
                self.linesProcessed += 1
                progress = round((self.linesProcessed * 100) / totalLinesInFile)
                if self.linesProcessed % 100 == 0 and progress != self.last_progress:
                    self.last_progress = progress
                    self.publish(f"Progress:{progress}")
                nextLine = in_file.readline()

            in_file.close()
            out_file.close()

        except Exception as e:
            self.publish(f"\nERROR PROCESSING FILE - {e}")
            self.errorProcessingFile = True

            # delete output file so it isn't accidentally used!
            try:
                os.remove(self.outputFilePath)
            except Exception as e:
                self.publish(f"Error deleting output file after error- {e}")

        if not self.endOfProgramFound:
            self.publish("WARNING - no end of program found (%, M2, or M30). G94 not inserted at the end of output file.")

        self.done()
        self.publish("Finished:Convert finished")

    def clear_all_data(self):
        self.errorMessage = ""
        self.errorProcessingFile = False
        self.linesProcessed = 0
        self.endOfProgramFound = False
        self.start_percent_found = False
        self.lastCoordinate = GCode_Coordinate(self)

        self.currentFeedRate = -1
        self.lastGCode = -1
        self.G17_18_19_Found = False
        self.G90Found = False
        self.G93Found = False
        self.G94Found = False
        self.totalToolPathDistance = 0
        self.totalDegreesRotated = 0

        self.totalG00lines = 0
        self.totalG01lines = 0
        self.totalG02lines = 0
        self.totalG03lines = 0

        self.rotaryMinMaxFound = False
        self.rotaryMax = 0
        self.rotaryMin = 0
        self.numberLinesWithRotaryMoves = 0
        
    def countLinesInFile(self, pathToFile):
        totalLines = 0
        try:
            infile = open(pathToFile)
            while infile.readline():
                totalLines += 1
            infile.close()
        except Exception as ignore:
            self.publish(f"Error counting lines in input file - {ignore}")
        
        return totalLines

    def processInputLine(self, lineIn):
        lineComments = ""
        commentStart = lineIn.find("(")
        if commentStart >= 0:
            lineComments = lineIn[commentStart:]
            lineIn = lineIn[:commentStart]
        lineIn = lineIn.upper()
        lineParts = lineIn.split()

        feedString = self.process_feed_rate(lineParts)
        self.process_non_moving(lineParts)
        moveContainsRotaryChangeAndNotG00 = self.process_moving_implicit(lineParts)

        StartFoundOnLine = self.process_start_stop(lineParts)

        # **** re-combine all the parts of the line ****
        returnLine = ""
        for part in lineParts:
            if len(part) > 0:
                if len(returnLine) > 0:
                    returnLine = returnLine + " " + part  # space needed between parts
                else:
                    returnLine = returnLine + part  # no space needed

        if self.wrap_all is False and not moveContainsRotaryChangeAndNotG00:
            if len(feedString) > 0:
                returnLine += (" " + feedString)

        if len(returnLine) > 0 and not returnLine.endswith(" ") and len(lineComments) > 0:
            returnLine = returnLine + " " + lineComments
        else:
            returnLine = returnLine + lineComments

        if StartFoundOnLine:
            conversionModeStr = "Use G93 mode for entire file" if self.wrap_all is True else "Wrap each rotary move in G93"

            returnLine += "\n( Converted G-code from G94 to G93 Inverse Time )"
            returnLine += "\n( WARNING: Review and test program for your machine and setup )"
            returnLine += "\n( Use this program at your own risk )"
            returnLine += f"\n( Z-zero offset from A-axis - {self.z0_offset} )"
            returnLine += f"\n( Conversion Mode - {conversionModeStr} )"
            returnLine += f"\n( Distance Units - {self.distUnits} )"
            self.publish("- Preamble added to output file")

        if self.wrap_all is False and moveContainsRotaryChangeAndNotG00:
            returnLine = "G93\n" + returnLine + "\nG94\nF" + str(self.currentFeedRate) + "\n"

        return returnLine.strip()

    def process_feed_rate(self, lineParts):
        feedStrings = ""
        
        for i in range(len(lineParts)):
            if lineParts[i].startswith("F"):
                feedStr = lineParts[i][1:]
                self.currentFeedRate = float(feedStr)
                feedStrings += (lineParts[i] + " ")
                del lineParts[i]
        return feedStrings

    def process_non_moving(self, lineParts):
        for i in range(len(lineParts)):
            if lineParts[i].startswith("G"):
                gCodeStr = lineParts[i][1:]
                gCode = int(gCodeStr)

                if gCode == 94:
                    self.G94Found = True
                    if self.wrap_all is True:
                        lineParts[i] = "G93"
                    self.publish(f"- G94 found on Line {self.linesProcessed + 1}")

                elif gCode == 93:
                    self.publish(f"- G93 found on Line {self.linesProcessed + 1}")
                    self.G93Found = True

                elif gCode == 17:
                    self.G17_18_19_Found = True
                    self.currentPlaneSelected = 17
                    self.publish(f"- G17 found on Line {self.linesProcessed + 1}")

                elif gCode == 18:
                    self.G17_18_19_Found = True 
                    self.currentPlaneSelected = 18
                    self.publish(f"- G18 found on Line {self.linesProcessed + 1}")

                elif gCode == 19:
                    self.G17_18_19_Found = True
                    self.currentPlaneSelected = 19
                    self.publish(f"- G19 found on Line {self.linesProcessed + 1}")

                elif gCode == 90:
                    self.G90Found = True
                    self.publish(f"- G90 found on Line {self.linesProcessed + 1}")

    def process_moving_implicit(self, lineParts):
        rotary_and_not_g00 = False
        moveCommandResult = self.check_for_move_command(lineParts)
        if moveCommandResult.is_movement_line is False:
            return False

        currentMoveGCode = -1
        if moveCommandResult.is_implicit_movement is True and self.lastGCode < 0:
            self.publish(f"Implicit move command given before (G0/1/2/3). (line {self.linesProcessed + 1})")
        currentMoveGCode = self.lastGCode if moveCommandResult.is_implicit_movement is True else moveCommandResult.gcode_movement_type

        self.lastGCode = currentMoveGCode

        if currentMoveGCode != 0:
            if self.currentFeedRate < 0:
                self.publish("Movement G-code (G01,G02,G03) found before the following requirements were set")
                self.publish("Feedrate not set")
            if self.G17_18_19_Found is False: self.publish("G17, G18 or G19 not set")

        endCoordinate = GCode_Coordinate(self, lineParts)
        endCoordinate.copy_unset_from_previous(self.lastCoordinate)

        distanceTraveled = -1
        degreesRotated = -1
        appendFvalueToLine = False

        if currentMoveGCode == 0:
            self.totalG00lines += 1
            distanceTraveled = endCoordinate.straight_distance(self.lastCoordinate, self.z0_offset, self.distUnits, False, self.linesProcessed + 1)
            degreesRotated = endCoordinate.rotationAbsoluteDifferenceDegrees(self.lastCoordinate)
            appendFvalueToLine = False

        elif currentMoveGCode == 1:
            self.totalG01lines += 1
            distanceTraveled = endCoordinate.straight_distance(self.lastCoordinate, self.z0_offset, self.distUnits, True, self.linesProcessed + 1)
            degreesRotated = endCoordinate.rotationAbsoluteDifferenceDegrees(self.lastCoordinate)
            appendFvalueToLine = True

        elif currentMoveGCode == 2:
            self.totalG02lines += 1
            distanceTraveled = endCoordinate.arc_distance_from_previous_coordinate(self.lastCoordinate, True, self.distUnits, self.linesProcessed + 1, self.currentPlaneSelected)
            degreesRotated = 0
            appendFvalueToLine = True

        elif currentMoveGCode == 3:
            self.totalG03lines += 1
            distanceTraveled = endCoordinate.arc_distance_from_previous_coordinate(self.lastCoordinate, False, self.distUnits, self.linesProcessed + 1, self.currentPlaneSelected)
            degreesRotated = 0
            appendFvalueToLine = True

        if endCoordinate.isAset() is True and self.lastCoordinate.isAset() is False:
            self.numberLinesWithRotaryMoves += 1

            if currentMoveGCode != 0:
                rotary_and_not_g00 = True

        elif endCoordinate.isAset() is True and endCoordinate.getA() != self.lastCoordinate.getA():
            self.numberLinesWithRotaryMoves += 1

            if currentMoveGCode != 0:
                rotary_and_not_g00 = True

        if appendFvalueToLine:
            if self.wrap_all is False:
                if rotary_and_not_g00 is False:
                    appendFvalueToLine = False  # we don't need to write the F rate in this case

        if appendFvalueToLine:
            if self.currentFeedRate <= 0:
                self.publish(f"Feedrate must be positive and non-zero (line {self.linesProcessed + 1})")

            timeForStepInMinutes = distanceTraveled / self.currentFeedRate

            frn = 0
            if timeForStepInMinutes > 0:
                frn = 1.0 / timeForStepInMinutes
            else:
                self.publish(f"WARNING - zero distance, zero speed, or infinite speed for move on line {self.linesProcessed + 1}")
            if frn <= 1.0:
                lineParts.append(f"F{frn:.2f}")
            else:
                lineParts.append(f"F{frn:.1f}")

        if distanceTraveled >= 0:
            self.totalToolPathDistance += distanceTraveled

        if degreesRotated >= 0:
            self.totalDegreesRotated += degreesRotated

        self.lastCoordinate = GCode_Coordinate(self, lineParts)
        self.lastCoordinate.copy_unset_from_previous(endCoordinate)

        if self.lastCoordinate.isAset() is True:
            if self.rotaryMinMaxFound is False:
                self.rotaryMax = self.lastCoordinate.getA()
                self.rotaryMin = self.lastCoordinate.getA()
                self.rotaryMinMaxFound = True
            else:
                if self.lastCoordinate.getA() > self.rotaryMax:
                    self.rotaryMax = self.lastCoordinate.getA()

                if self.lastCoordinate.getA() < self.rotaryMin:
                    self.rotaryMin = self.lastCoordinate.getA()

        return rotary_and_not_g00

    def check_for_move_command(self, lineParts):
        movementKeys = ["G0", "G00", "G01", "G1", "G02", "G2", "G03", "G3"]
        implicitKeys = ["X", "Y", "Z", "A"]
        for part in lineParts:
            if part in movementKeys:
                return GCodeMovementResult(True, False, part)

            for key in implicitKeys:
                if part.startswith(key):
                    return GCodeMovementResult(True, True, "")

        return GCodeMovementResult(False, False, "")

    def process_start_stop(self, lineParts):
        percent_found = False

        for i in range(len(lineParts)):
            if lineParts[i].startswith("%"):
                if not self.start_percent_found:
                    percent_found = True
                    self.start_percent_found = True
                    self.publish(f"- Start of program found (%) on line {self.linesProcessed + 1}")
                elif not self.endOfProgramFound:
                    lineParts[i] = f"\nG94 (Revert to G94 mode)\nF{self.currentFeedRate} (last feed rate used)\n{lineParts[i]}"
                    self.publish(f"- End of program found (%) on line {self.linesProcessed + 1}")
                    self.endOfProgramFound = True

            if lineParts[i].startswith("M2") or lineParts[i].startswith("M30"):
                if not self.endOfProgramFound:
                    lineParts[i] = f"\nG94 (Revert to G94 mode)\nF{self.currentFeedRate} (last feed rate used)\n{lineParts[i]}"
                    self.publish(f"- End of program found (M2 and/or M30) on line {self.linesProcessed + 1}")
                    self.endOfProgramFound = True

        return percent_found

    def publish(self, message):
        self.socket.send(message.encode('utf-8'))
        message = self.socket.recv()

    def done(self):
        if not self.last_progress == 100:
            self.publish("Progress:100")
        if self.numberLinesWithRotaryMoves == 0:
            self.publish("Error:This is not a wrapped gcode file.")
            return
        if self.G90Found is False:
            self.publish("Error:No G90 commands were found")
            return
        if self.G93Found is True:
            self.publish("Error:Retaining G93 mode within input file is not supported.")
            return
        if self.G94Found is False:
            self.publish("Error:No G94 commands were found")
            return
        try:
            durationSeconds = time.time() - self.startTime

            self.totalG0123Lines = self.totalG00lines + self.totalG01lines + self.totalG02lines + self.totalG03lines
            fractionLinesWithAM = float(self.numberLinesWithRotaryMoves / self.totalG0123Lines) if self.totalG0123Lines != 0 else 0.0
            self.publish(" ")
            self.publish(f"Number of G00 lines - {self.totalG00lines}")
            self.publish(f"Number of G01 lines - {self.totalG01lines}")
            self.publish(f"Number of G02 lines - {self.totalG02lines}")
            self.publish(f"Number of G03 lines - {self.totalG03lines}")
            self.publish(f"Total G00+G01+G02+G03 lines - {self.totalG0123Lines}")
            self.publish(f"Number of lines with rotary moves - {self.numberLinesWithRotaryMoves} ({fractionLinesWithAM * 100:.1f}%)")
            if self.rotaryMinMaxFound is True:
                self.publish(f"Rotary (A-axis) Min/Max rotations from {self.rotaryMin} to {self.rotaryMax}")
            else:
                self.publish("No Rotary Min/Max found")
            self.publish(f"Total toolpath distance (including rotary moves) - {self.totalToolPathDistance:.3f} {self.distUnits}")
            self.publish(f"Total degrees of rotation for A-axis - {self.totalDegreesRotated:.3f} deg")
            self.publish("--------------------------")
            self.publish(f"Completed in {durationSeconds:.3f} seconds")

            # helpful hints regarding selection of conversion mode
            if self.totalG0123Lines > 50:
                if self.wrap_all is True and fractionLinesWithAM < 0.15:
                    self.publish("Only a small percentage of the lines in the input file had rotary moves.")
                    self.publish("It may be more efficient to wrap each rotary move instead of converting the entire file.")

                if self.wrap_all is False and fractionLinesWithAM > 0.25:
                    self.publish("A sizeable percentage of the lines in the input file had rotary moves.")
                    self.publish("It may be more efficient to wrap the entire file instead of wrapping each rotary move.")

        except Exception:
            pass

    def send_error(self, error):
        sys.stderr.write(error + "\n")
        sys.stderr.flush()
        
if __name__ == "__main__":
    app = File_Converter(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5])
