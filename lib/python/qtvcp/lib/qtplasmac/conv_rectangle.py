'''
conv_rectangle.py

Copyright (C) 2020, 2021  Phillip A Carter
Copyright (C) 2020, 2021  Gregory D Carl

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
'''

import math
from PyQt5.QtCore import Qt, QCoreApplication
from PyQt5.QtWidgets import QLabel, QLineEdit, QPushButton, QRadioButton, QButtonGroup, QMessageBox
from PyQt5.QtGui import QPixmap

_translate = QCoreApplication.translate

def preview(P, W):
    if P.dialogError: return
    xLB = yLR = xLT = yLL = 0
    if W.r1Button.text().split()[0] == _translate('Conversational', 'RADIUS'):
        r1Type = 'radius'
    elif W.r1Button.text().split()[0] == _translate('Conversational', 'CHAMFER'):
        r1Type = 'chamfer'
    else:
        r1Type = 'internal'
    if W.r2Button.text().split()[0] == _translate('Conversational', 'RADIUS'):
        r2Type = 'radius'
    elif W.r2Button.text().split()[0] == _translate('Conversational', 'CHAMFER'):
        r2Type = 'chamfer'
    else:
        r2Type = 'internal'
    if W.r3Button.text().split()[0] == _translate('Conversational', 'RADIUS'):
        r3Type = 'radius'
    elif W.r3Button.text().split()[0] == _translate('Conversational', 'CHAMFER'):
        r3Type = 'chamfer'
    else:
        r3Type = 'internal'
    if W.r4Button.text().split()[0] == _translate('Conversational', 'RADIUS'):
        r4Type = 'radius'
    elif W.r4Button.text().split()[0] == _translate('Conversational', 'CHAMFER'):
        r4Type = 'chamfer'
    else:
        r4Type = 'internal'
    if W.xlEntry.text() and W.ylEntry.text():
        try:
            if float(W.xlEntry.text()) <= 0 or float(W.ylEntry.text()) <= 0:
                msg0 = _translate('Conversational', 'A positive X LENGTH is required')
                msg1 = _translate('Conversational', 'AND')
                msg2 = _translate('Conversational', 'A positive Y LENGTH is required')
                error_set(P, '{}.\n\n{}\n\n{}.\n'.format(msg0, msg1, msg2))
                return
        except:
            msg0 = _translate('Conversational', 'Invalid X LENGTH or Y LENGTH entry detected.\n')
            error_set(P, '{}.\n'.format(msg0))
            return
        try:
            if W.r1Entry.text():
                radius1 = float(W.r1Entry.text())
            else:
                radius1 = 0.0
            if W.r2Entry.text():
                radius2 = float(W.r2Entry.text())
            else:
                radius2 = 0.0
            if W.r3Entry.text():
                radius3 = float(W.r3Entry.text())
            else:
                radius3 = 0.0
            if W.r4Entry.text():
                radius4 = float(W.r4Entry.text())
            else:
                radius4 = 0.0
        except:
            msg0 = _translate('Conversational', 'Invalid RADIUS entry detected')
            error_set(P, '{}.\n'.format(msg0))
            return
        if radius1 + radius2 > float(W.xlEntry.text()):
            msg0 = _translate('Conversational', 'Radius 1 plus Radius 2')
            msg1 = _translate('Conversational', 'cannot be greater than')
            error_set(P, '{} ({})\n\n{} {}\n'.format(msg0, radius1 + radius2, msg1, float(W.xlEntry.text())))
            return
        if radius1 + radius3 > float(W.ylEntry.text()):
            msg0 = _translate('Conversational', 'Radius 1 plus Radius 3')
            msg1 = _translate('Conversational', 'cannot be greater than')
            error_set(P, '{} ({})\n\n{} {}\n'.format(msg0, radius1 + radius3, msg1, float(W.ylEntry.text())))
            return
        if radius2 + radius4 > float(W.ylEntry.text()):
            msg0 = _translate('Conversational', 'Radius 2 plus Radius 4')
            msg1 = _translate('Conversational', 'can not be greater than')
            error_set(P, '{} ({})\n\n{} {}\n'.format(msg0, radius2 + radius4, msg1, float(W.ylEntry.text())))
            return
        if radius3 > float(W.xlEntry.text()) / 2 or radius4 > float(W.xlEntry.text()) / 2:
            msg0 = _translate('Conversational', 'Neither Radius 3 nor Radius 4')
            msg1 = _translate('Conversational', 'can be greater than')
            error_set(P, '{}\n\n{} {}\n'.format(msg0, msg1, float(W.xlEntry.text()) /2 ))
            return
        if W.xlEntry.text():
            xLB = float(W.xlEntry.text()) - (radius3 + radius4)
            xLT = float(W.xlEntry.text()) - (radius1 + radius2)
            xC = float(W.xlEntry.text()) / 2
        if W.ylEntry.text():
            yLR = float(W.ylEntry.text()) - (radius2 + radius4)
            yLL = float(W.ylEntry.text()) - (radius1 + radius3)
            yC = float(W.ylEntry.text()) / 2
        if xLB >= 0 and yLR >= 0 and xLT >= 0 and yLL >= 0:
            blLength = math.sqrt(xC ** 2 + (yC * 2) ** 2)
            blAngle = math.atan((yC * 2) / xC)
            try:
                if W.angEntry.text():
                    angle = math.radians(float(W.angEntry.text()))
                else:
                    angle = 0.0
            except:
                msg0 = _translate('Conversational', 'Invalid ANGLE entry detected')
                error_set(P, '{}.\n'.format(msg0))
                return
            try:
                if W.liEntry.text():
                    leadInOffset = math.sin(math.radians(45)) * float(W.liEntry.text())
                else:
                    leadInOffset = 0
            except:
                msg = _translate('Conversational', 'Invalid LEAD IN entry detected')
                error_set(P, '{}.\n'.format(msg0))
                return
            try:
                if W.loEntry.text():
                    leadOutOffset = math.sin(math.radians(45)) * float(W.loEntry.text())
                else:
                    leadOutOffset = 0
            except:
                msg = _translate('Conversational', 'Invalid LEAD OUT entry detected')
                error_set(P, '{}.\n'.format(msg0))
                return
            right = math.radians(0)
            up = math.radians(90)
            left = math.radians(180)
            down = math.radians(270)
            try:
                kOffset = float(W.kerf_width.value()) * W.kOffset.isChecked() / 2
            except:
                msg = _translate('Conversational', 'Invalid Kerf Width entry in material detected.')
                error_set(P, '{}.\n'.format(msg0))
                return
            if not W.xsEntry.text():
                W.xsEntry.setText('{:0.3f}'.format(P.xOrigin))
            msg0 = _translate('Conversational', 'Invalid entry detected in')
            msg1 = _translate('Conversational', 'ORIGIN')
            try:
                if W.center.isChecked():
                    if W.cExt.isChecked():
                        xS = float(W.xsEntry.text()) + ((yC - radius2) * math.cos(angle + up)) + (xC * math.cos(angle + right))
                    else:
                        xS = float(W.xsEntry.text()) + yC * math.cos(angle + up)
                else:
                    if W.cExt.isChecked():
                        xS = (float(W.xsEntry.text()) + kOffset) + (float(W.xlEntry.text()) * math.cos(angle + up)) + (float(W.xlEntry.text()) * math.cos(angle + right))
                    else:
                        xS = (float(W.xsEntry.text()) - kOffset) + (blLength * math.cos(angle + right + blAngle))
            except:
                error_set(P, '{}:\n\nX {}\n'.format(msg0, msg1))
                return
            if not W.ysEntry.text():
                W.ysEntry.setText('{:0.3f}'.format(P.yOrigin))
            try:
                if W.center.isChecked():
                    if W.cExt.isChecked():
                        yS = float(W.ysEntry.text()) + (yC - radius2 * math.sin(angle + up)) + (xC * math.sin(angle + right))
                    else:
                        yS = float(W.ysEntry.text()) + yC * math.sin(angle + up)
                else:
                    if W.cExt.isChecked():
                        yS = (float(W.ysEntry.text()) + kOffset) + ((float(W.ylEntry.text()) - radius2) * math.sin(angle + up)) + (float(W.xlEntry.text()) * math.sin(angle + right))
                    else:
                        yS = (float(W.ysEntry.text()) - kOffset) + (blLength * math.sin(angle + right + blAngle))
            except:
                error_set(P, '{}:\n\nY {}\n'.format(msg0, msg1))
                return
            outTmp = open(P.fTmp, 'w')
            outNgc = open(P.fNgc, 'w')
            inWiz = open(P.fNgcBkp, 'r')
            for line in inWiz:
                if '(new conversational file)' in line:
                    outNgc.write('\n{} (preamble)\n'.format(P.preAmble))
                    break
                elif '(postamble)' in line:
                    break
                elif 'm2' in line.lower() or 'm30' in line.lower():
                    break
                outNgc.write(line)
            outTmp.write('\n(conversational rectangle)\n')
            outTmp.write('M190 P{}\n'.format(int(W.conv_material.currentText().split(':')[0])))
            outTmp.write('M66 P3 L3 Q1\n')
            outTmp.write('f#<_hal[plasmac.cut-feed-rate]>\n')
            if W.cExt.isChecked():
                if leadInOffset > 0:
                    xlCentre = xS + (leadInOffset * math.cos(angle + right))
                    ylCentre = yS + (leadInOffset * math.sin(angle + right))
                    xlStart = xlCentre + (leadInOffset * math.cos(angle + up))
                    ylStart = ylCentre + (leadInOffset * math.sin(angle + up))
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                    if W.kOffset.isChecked():
                        outTmp.write('g41.1 d#<_hal[qtplasmac.kerf_width-f]>\n')
                    outTmp.write('m3 $0 s1\n')
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
                else:
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
                    outTmp.write('m3 $0 s1\n')
                x1 = xS + yLR * math.cos(angle + down)
                y1 = yS + yLR * math.sin(angle + down)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
                if radius4 > 0:
                    if r4Type == 'internal':
                        xrCentre = x1 + (radius4 * math.cos(angle + down))
                        yrCentre = y1 + (radius4 * math.sin(angle + down))
                        xrEnd = xrCentre + (radius4 * math.cos(angle + left))
                        yrEnd = yrCentre + (radius4 * math.sin(angle + left))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                    else:
                        xrCentre = x1 + (radius4 * math.cos(angle + left))
                        yrCentre = y1 + (radius4 * math.sin(angle + left))
                        xrEnd = xrCentre + (radius4 * math.cos(angle + down))
                        yrEnd = yrCentre + (radius4 * math.sin(angle + down))
                    if r4Type == 'radius':
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                    x2 = xrEnd + xLB * math.cos(angle + left)
                    y2 = yrEnd + xLB * math.sin(angle + left)
                else:
                    x2 = x1 + xLB * math.cos(angle + left)
                    y2 = y1 + xLB * math.sin(angle + left)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                if radius3 > 0:
                    if r3Type == 'internal':
                        xrCentre = x2 + (radius3 * math.cos(angle + left))
                        yrCentre = y2 + (radius3 * math.sin(angle + left))
                        xrEnd = xrCentre + (radius3 * math.cos(angle + up))
                        yrEnd = yrCentre + (radius3 * math.sin(angle + up))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                    else:
                        xrCentre = x2 + (radius3 * math.cos(angle + up))
                        yrCentre = y2 + (radius3 * math.sin(angle + up))
                        xrEnd = xrCentre + (radius3 * math.cos(angle + left))
                        yrEnd = yrCentre + (radius3 * math.sin(angle + left))
                    if r3Type == 'radius':
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                    x3 = xrEnd + yLL * math.cos(angle + up)
                    y3 = yrEnd + yLL * math.sin(angle + up)
                else:
                    x3 = x2 + yLL * math.cos(angle + up)
                    y3 = y2 + yLL * math.sin(angle + up)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                if radius1 > 0:
                    if r1Type == 'internal':
                        xrCentre = x3 + (radius1 * math.cos(angle + up))
                        yrCentre = y3 + (radius1 * math.sin(angle + up))
                        xrEnd = xrCentre + (radius1 * math.cos(angle + right))
                        yrEnd = yrCentre + (radius1 * math.sin(angle + right))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                    else:
                        xrCentre = x3 + (radius1 * math.cos(angle + right))
                        yrCentre = y3 + (radius1 * math.sin(angle + right))
                        xrEnd = xrCentre + (radius1 * math.cos(angle + up))
                        yrEnd = yrCentre + (radius1 * math.sin(angle + up))
                    if r1Type == 'radius':
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                    x4 = xrEnd + xLT * math.cos(angle + right)
                    y4 = yrEnd + xLT * math.sin(angle + right)
                else:
                    x4 = x3 + xLT * math.cos(angle + right)
                    y4 = y3 + xLT * math.sin(angle + right)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                if radius2 > 0:
                    if r2Type == 'internal':
                        xrCentre = x4 + (radius2 * math.cos(angle + right))
                        yrCentre = y4 + (radius2 * math.sin(angle + right))
                        xrEnd = xrCentre + (radius2 * math.cos(angle + down))
                        yrEnd = yrCentre + (radius2 * math.sin(angle + down))
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                    else:
                        xrCentre = x4 + (radius2 * math.cos(angle + down))
                        yrCentre = y4 + (radius2 * math.sin(angle + down))
                        xrEnd = xrCentre + (radius2 * math.cos(angle + right))
                        yrEnd = yrCentre + (radius2 * math.sin(angle + right))
                    if r2Type == 'radius':
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            else:
                if leadInOffset > 0:
                    xlCentre = xS + (leadInOffset * math.cos(angle + down))
                    ylCentre = yS + (leadInOffset * math.sin(angle + down))
                    xlStart = xlCentre + (leadInOffset * math.cos(angle + right))
                    ylStart = ylCentre + (leadInOffset * math.sin(angle + right))
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xlStart, ylStart))
                    if W.kOffset.isChecked():
                        outTmp.write('g41.1 d#<_hal[qtplasmac.kerf_width-f]>\n')
                    outTmp.write('m3 $0 s1\n')
                    outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xS, yS , xlCentre - xlStart, ylCentre - ylStart))
                else:
                    outTmp.write('g0 x{:.6f} y{:.6f}\n'.format(xS, yS))
                    outTmp.write('m3 $0 s1\n')
                x1 = xS + (xLT / 2) * math.cos(angle + left)
                y1 = yS + (xLT / 2) * math.sin(angle + left)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x1, y1))
                if radius1 > 0:
                    if r1Type == 'internal':
                        xrCentre = x1 + (radius1 * math.cos(angle + left))
                        yrCentre = y1 + (radius1 * math.sin(angle + left))
                        xrEnd = xrCentre + (radius1 * math.cos(angle + down))
                        yrEnd = yrCentre + (radius1 * math.sin(angle + down))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                    else:
                        xrCentre = x1 + (radius1 * math.cos(angle + down))
                        yrCentre = y1 + (radius1 * math.sin(angle + down))
                        xrEnd = xrCentre + (radius1 * math.cos(angle + left))
                        yrEnd = yrCentre + (radius1 * math.sin(angle + left))
                    if r1Type == 'radius':
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x1, yrCentre - y1))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                    x2 = xrEnd + yLL * math.cos(angle + down)
                    y2 = yrEnd + yLL * math.sin(angle + down)
                else:
                    x2 = x1 + yLL * math.cos(angle + down)
                    y2 = y1 + yLL * math.sin(angle + down)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x2, y2))
                if radius3 > 0:
                    if r3Type == 'internal':
                        xrCentre = x2 + (radius3 * math.cos(angle + down))
                        yrCentre = y2 + (radius3 * math.sin(angle + down))
                        xrEnd = xrCentre + (radius3 * math.cos(angle + right))
                        yrEnd = yrCentre + (radius3 * math.sin(angle + right))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                    else:
                        xrCentre = x2 + (radius3 * math.cos(angle + right))
                        yrCentre = y2 + (radius3 * math.sin(angle + right))
                        xrEnd = xrCentre + (radius3 * math.cos(angle + down))
                        yrEnd = yrCentre + (radius3 * math.sin(angle + down))
                    if r3Type == 'radius':
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x2, yrCentre - y2))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                    x3 = xrEnd + xLB * math.cos(angle + right)
                    y3 = yrEnd + xLB * math.sin(angle + right)
                else:
                    x3 = x2 + xLB * math.cos(angle + right)
                    y3 = y2 + xLB * math.sin(angle + right)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x3, y3))
                if radius4 > 0:
                    if r4Type == 'internal':
                        xrCentre = x3 + (radius4 * math.cos(angle + right))
                        yrCentre = y3 + (radius4 * math.sin(angle + right))
                        xrEnd = xrCentre + (radius4 * math.cos(angle + up))
                        yrEnd = yrCentre + (radius4 * math.sin(angle + up))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                    else:
                        xrCentre = x3 + (radius4 * math.cos(angle + up))
                        yrCentre = y3 + (radius4 * math.sin(angle + up))
                        xrEnd = xrCentre + (radius4 * math.cos(angle + right))
                        yrEnd = yrCentre + (radius4 * math.sin(angle + right))
                    if r4Type == 'radius':
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x3, yrCentre - y3))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
                    x4 = xrEnd + yLR * math.cos(angle + up)
                    y4 = yrEnd + yLR * math.sin(angle + up)
                else:
                    x4 = x3 + yLR * math.cos(angle + up)
                    y4 = y3 + yLR * math.sin(angle + up)
                outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(x4, y4))
                if radius2 > 0:
                    if r2Type == 'internal':
                        xrCentre = x4 + (radius2 * math.cos(angle + up))
                        yrCentre = y4 + (radius2 * math.sin(angle + up))
                        xrEnd = xrCentre + (radius2 * math.cos(angle + left))
                        yrEnd = yrCentre + (radius2 * math.sin(angle + left))
                        outTmp.write('g2 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                    else:
                        xrCentre = x4 + (radius2 * math.cos(angle + left))
                        yrCentre = y4 + (radius2 * math.sin(angle + left))
                        xrEnd = xrCentre + (radius2 * math.cos(angle + up))
                        yrEnd = yrCentre + (radius2 * math.sin(angle + up))
                    if r2Type == 'radius':
                        outTmp.write('g3 x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(xrEnd, yrEnd, xrCentre - x4, yrCentre - y4))
                    else:
                        outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xrEnd, yrEnd))
            outTmp.write('g1 x{:.6f} y{:.6f}\n'.format(xS, yS))
            if leadOutOffset > 0: # and not (W.cExt.isChecked() and radius2):
                if W.cExt.isChecked() and not radius2:
                    dir = ['g2', down, right]
                elif W.cExt.isChecked() and radius2:
                    dir = ['g3', right, down]
                else:
                    dir = ['g3', down, left]
                xlCentre = xS + (leadOutOffset * math.cos(angle + dir[1]))
                ylCentre = yS + (leadOutOffset * math.sin(angle + dir[1]))
                xlEnd = xlCentre + (leadOutOffset * math.cos(angle + dir[2]))
                ylEnd = ylCentre + (leadOutOffset * math.sin(angle + dir[2]))
                outTmp.write('{} x{:.6f} y{:.6f} i{:.6f} j{:.6f}\n'.format(dir[0], xlEnd, ylEnd , xlCentre - xS, ylCentre - yS))
            outTmp.write('g40\n')
            outTmp.write('m5 $0\n')
            outTmp.close()
            outTmp = open(P.fTmp, 'r')
            for line in outTmp:
                outNgc.write(line)
            outTmp.close()
            outNgc.write('\n{} (postamble)\n'.format(P.postAmble))
            outNgc.write('m2\n')
            outNgc.close()
            W.conv_preview.load(P.fNgc)
            W.conv_preview.set_current_view()
            W.add.setEnabled(True)
        W.undo.setEnabled(True)
        P.conv_preview_button(True)
    else:
        msg0 = _translate('Conversational', 'A positive value is required for')
        msg1 = _translate('Conversational', 'LENGTH')
        error_set(P, '{0}:\n\nX {1}\nY {1}\n'.format(msg0, msg1))

def error_set(P, msg):
    P.dialogError = True
    P.dialog_show_ok(QMessageBox.Warning, _translate('Conversational', 'Rectangle Error'), msg)

def rad_button_pressed(P, W, button, value):

    if button.text().split()[0] == _translate('Conversational', 'RADIUS'):
        text = _translate('Conversational', 'CHAMFER')
        button.setText('{} {}'.format(text, value))

    elif button.text().split()[0] == _translate('Conversational', 'CHAMFER'):
        text = _translate('Conversational', 'iRADIUS')
        button.setText('{} {}'.format(text, value))

    else:
        text = _translate('Conversational', 'RADIUS')
        button.setText('{} {}'.format(text, value))
    auto_preview(P, W)

def entry_changed(P, W, widget):
    char = P.conv_entry_changed(widget)
    msg = []
    try:
        li = float(W.liEntry.text())
    except:
        msg.append(_translate('Conversational', 'LEADIN'))
    try:
        kw = float(W.kerf_width.value())
    except:
        msg.append(_translate('Conversational', 'KERF'))
    if msg:
        msg0 = _translate('Conversational', 'Invalid entry detected in')
        msg1 = ''
        for m in msg:
            msg1 += '{}\n'.format(m)
        error_set(P, '{}:\n\n{}'.format(msg0, msg1))
        return
    if char == "operator" or not W.liEntry.text() or li == 0 or li <= kw / 2:
        W.kOffset.setEnabled(False)
        W.kOffset.setChecked(False)
    else:
        W.kOffset.setEnabled(True)

def auto_preview(P, W):
    if W.main_tab_widget.currentIndex() == 1 and \
       W.xlEntry.text() and W.ylEntry.text():
        preview(P, W)

def widgets(P, W):
    W.spGroup = QButtonGroup(W)
    W.center = QRadioButton(_translate('Conversational', 'CENTER'))
    W.spGroup.addButton(W.center)
    W.bLeft = QRadioButton(_translate('Conversational', 'BTM LEFT'))
    W.spGroup.addButton(W.bLeft)
    W.liLabel = QLabel(_translate('Conversational', 'LEAD IN'))
    W.liEntry = QLineEdit(str(P.leadIn), objectName = 'liEntry')
    W.loLabel = QLabel(_translate('Conversational', 'LEAD OUT'))
    W.loEntry = QLineEdit(str(P.leadOut), objectName = 'loEntry')
    if not P.convSettingsChanged:
        #widgets
        W.ctLabel = QLabel(_translate('Conversational', 'CUT TYPE'))
        W.ctGroup = QButtonGroup(W)
        W.cExt = QRadioButton(_translate('Conversational', 'EXTERNAL'))
        W.cExt.setChecked(True)
        W.ctGroup.addButton(W.cExt)
        W.cInt = QRadioButton(_translate('Conversational', 'INTERNAL'))
        W.ctGroup.addButton(W.cInt)
        W.koLabel = QLabel(_translate('Conversational', 'KERF'))
        W.kOffset = QPushButton(_translate('Conversational', 'OFFSET'))
        W.kOffset.setCheckable(True)
        W.spLabel = QLabel(_translate('Conversational', 'START'))
        text = _translate('Conversational', 'ORIGIN')
        W.xsLabel = QLabel(_translate('Conversational', 'X {}'.format(text)))
        W.xsEntry = QLineEdit(str(P.xSaved), objectName = 'xsEntry')
        W.ysLabel = QLabel(_translate('Conversational', 'Y {}'.format(text)))
        W.ysEntry = QLineEdit(str(P.ySaved), objectName = 'ysEntry')
        text = _translate('Conversational', 'LENGTH')
        W.xlLabel = QLabel(_translate('Conversational', 'X {}'.format(text)))
        W.xlEntry = QLineEdit()
        W.ylLabel = QLabel(_translate('Conversational', 'Y {}'.format(text)))
        W.ylEntry = QLineEdit()
        W.angLabel = QLabel(_translate('Conversational', 'ANGLE'))
        W.angEntry = QLineEdit('0.0', objectName='aEntry')
        text = _translate('Conversational', 'RADIUS')
        W.r1Button = QPushButton(_translate('Conversational', '{} 1'.format(text)))
        W.r1Entry = QLineEdit()
        W.r2Button = QPushButton(_translate('Conversational', '{} 2'.format(text)))
        W.r2Entry = QLineEdit()
        W.r3Button = QPushButton(_translate('Conversational', '{} 3'.format(text)))
        W.r3Entry = QLineEdit()
        W.r4Button = QPushButton(_translate('Conversational', '{} 4'.format(text)))
        W.r4Entry = QLineEdit()
    W.add = QPushButton(_translate('Conversational', 'ADD'))
    W.lDesc = QLabel(_translate('Conversational', 'CREATING RECTANGLE'))
    W.iLabel = QLabel()
    pixmap = QPixmap('{}conv_rectangle_l.png'.format(P.IMAGES)).scaledToWidth(196)
    W.iLabel.setPixmap(pixmap)
    #alignment and size
    rightAlign = ['ctLabel', 'koLabel', 'spLabel', 'xsLabel', 'xsEntry', 'ysLabel', \
                  'ysEntry', 'liLabel', 'liEntry', 'loLabel', 'loEntry', 'xlLabel', \
                  'xlEntry', 'ylLabel', 'ylEntry', 'angLabel', 'angEntry', 'r1Entry', \
                  'r2Entry', 'r3Entry', 'r4Entry']
    centerAlign = ['lDesc']
    rButton = ['cExt', 'cInt', 'center', 'bLeft']
    pButton = ['preview', 'add', 'undo', 'kOffset', \
               'r1Button', 'r2Button', 'r3Button', 'r4Button']
    for widget in rightAlign:
        W[widget].setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in centerAlign:
        W[widget].setAlignment(Qt.AlignCenter | Qt.AlignBottom)
        W[widget].setFixedWidth(240)
        W[widget].setFixedHeight(24)
    for widget in rButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    for widget in pButton:
        W[widget].setFixedWidth(80)
        W[widget].setFixedHeight(24)
    #starting parameters
    W.add.setEnabled(False)
    if P.oSaved:
        W.center.setChecked(True)
    else:
        W.bLeft.setChecked(True)
    if not W.liEntry.text() or float(W.liEntry.text()) == 0:
        W.kOffset.setChecked(False)
        W.kOffset.setEnabled(False)
    #connections
    W.preview.pressed.disconnect()
    W.undo.pressed.disconnect()
    W.conv_material.currentTextChanged.connect(lambda:auto_preview(P, W))
    W.cExt.toggled.connect(lambda:auto_preview(P, W))
    W.kOffset.toggled.connect(lambda:auto_preview(P, W))
    W.center.toggled.connect(lambda:auto_preview(P, W))
    W.preview.pressed.connect(lambda:preview(P, W))
    W.add.pressed.connect(lambda:P.conv_add_shape_to_file())
    W.undo.pressed.connect(lambda:P.conv_undo_shape())
    entries = ['xsEntry', 'ysEntry', 'liEntry', 'loEntry', 'xlEntry', 'ylEntry', \
               'angEntry', 'r1Entry', 'r2Entry', 'r3Entry', 'r4Entry', ]
    for entry in entries:
        W[entry].textChanged.connect(lambda:entry_changed(P, W, W.sender()))
        W[entry].returnPressed.connect(lambda:preview(P, W))
    W.r1Button.pressed.connect(lambda:rad_button_pressed(P, W, W.sender(), '1'))
    W.r2Button.pressed.connect(lambda:rad_button_pressed(P, W, W.sender(), '2'))
    W.r3Button.pressed.connect(lambda:rad_button_pressed(P, W, W.sender(), '3'))
    W.r4Button.pressed.connect(lambda:rad_button_pressed(P, W, W.sender(), '4'))
    #add to layout
    if P.landscape:
        W.entries.addWidget(W.ctLabel, 0, 0)
        W.entries.addWidget(W.cExt, 0, 1)
        W.entries.addWidget(W.cInt, 0, 2)
        W.entries.addWidget(W.koLabel, 0, 3)
        W.entries.addWidget(W.kOffset, 0, 4)
        W.entries.addWidget(W.spLabel, 1, 0)
        W.entries.addWidget(W.center, 1, 1)
        W.entries.addWidget(W.bLeft, 1, 2)
        W.entries.addWidget(W.xsLabel, 2, 0)
        W.entries.addWidget(W.xsEntry, 2, 1)
        W.entries.addWidget(W.ysLabel, 3, 0)
        W.entries.addWidget(W.ysEntry, 3, 1)
        W.entries.addWidget(W.liLabel, 4, 0)
        W.entries.addWidget(W.liEntry, 4, 1)
        W.entries.addWidget(W.loLabel, 5, 0)
        W.entries.addWidget(W.loEntry, 5, 1)
        W.entries.addWidget(W.xlLabel, 6, 0)
        W.entries.addWidget(W.xlEntry, 6, 1)
        W.entries.addWidget(W.ylLabel, 7, 0)
        W.entries.addWidget(W.ylEntry, 7, 1)
        W.entries.addWidget(W.angLabel, 8, 0)
        W.entries.addWidget(W.angEntry, 8, 1)
        W.entries.addWidget(W.r1Button, 9, 0)
        W.entries.addWidget(W.r1Entry, 9, 1)
        W.entries.addWidget(W.r2Button, 9, 2)
        W.entries.addWidget(W.r2Entry, 9, 3)
        W.entries.addWidget(W.r3Button, 10, 0)
        W.entries.addWidget(W.r3Entry, 10, 1)
        W.entries.addWidget(W.r4Button, 10, 2)
        W.entries.addWidget(W.r4Entry, 10, 3)
        for r in [11]:
            W['s{}'.format(r)] = QLabel('')
            W['s{}'.format(r)].setFixedHeight(24)
            W.entries.addWidget(W['s{}'.format(r)], r, 0)
        W.entries.addWidget(W.preview, 12, 0)
        W.entries.addWidget(W.add, 12, 2)
        W.entries.addWidget(W.undo, 12, 4)
        W.entries.addWidget(W.lDesc, 13 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 2 , 2, 7, 3)
    else:
        W.entries.addWidget(W.conv_material, 0, 0, 1, 5)
        W.entries.addWidget(W.ctLabel, 1, 0)
        W.entries.addWidget(W.cExt, 1, 1)
        W.entries.addWidget(W.cInt, 1, 2)
        W.entries.addWidget(W.koLabel, 1, 3)
        W.entries.addWidget(W.kOffset, 1, 4)
        W.entries.addWidget(W.spLabel, 2, 0)
        W.entries.addWidget(W.center, 2, 1)
        W.entries.addWidget(W.bLeft, 2, 2)
        W.entries.addWidget(W.xsLabel, 3, 0)
        W.entries.addWidget(W.xsEntry, 3, 1)
        W.entries.addWidget(W.ysLabel, 3, 2)
        W.entries.addWidget(W.ysEntry, 3, 3)
        W.entries.addWidget(W.liLabel, 4, 0)
        W.entries.addWidget(W.liEntry, 4, 1)
        W.entries.addWidget(W.loLabel, 4, 2)
        W.entries.addWidget(W.loEntry, 4, 3)
        W.entries.addWidget(W.xlLabel, 5, 0)
        W.entries.addWidget(W.xlEntry, 5, 1)
        W.entries.addWidget(W.ylLabel, 5, 2)
        W.entries.addWidget(W.ylEntry, 5, 3)
        W.entries.addWidget(W.angLabel, 6, 0)
        W.entries.addWidget(W.angEntry, 6, 1)
        W.entries.addWidget(W.r1Button, 7, 0)
        W.entries.addWidget(W.r1Entry, 7, 1)
        W.entries.addWidget(W.r2Button, 7, 2)
        W.entries.addWidget(W.r2Entry, 7, 3)
        W.entries.addWidget(W.r3Button, 8, 0)
        W.entries.addWidget(W.r3Entry, 8, 1)
        W.entries.addWidget(W.r4Button, 8, 2)
        W.entries.addWidget(W.r4Entry, 8, 3)
        W.entries.addWidget(W.preview, 9, 0)
        W.entries.addWidget(W.add, 9, 2)
        W.entries.addWidget(W.undo, 9, 4)
        W.entries.addWidget(W.lDesc, 10 , 1, 1, 3)
        W.entries.addWidget(W.iLabel, 0 , 5, 7, 3)
    W.xlEntry.setFocus()
    P.convSettingsChanged = False
