
'''
qtplasmac-materials.py

Copyright (C) 2020 - 2024 Phillip A Carter
Copyright (C) 2020 - 2024 Gregory D Carl

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

import os
import sys
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from PyQt5.QtGui import *


class MaterialConverter(QMainWindow, object):

    def __init__(self, parent=None):
        super(MaterialConverter, self).__init__(parent)
        wid = QWidget(self)
        self.setCentralWidget(wid)
        self.layout = QHBoxLayout()
        wid.setLayout(self.layout)
        self.iconPath = 'share/icons/hicolor/scalable/apps/linuxcnc_alt/linuxcncicon_plasma.svg'
        appPath = os.path.realpath(os.path.dirname(sys.argv[0]))
        self.iconBase = '/usr' if appPath == '/usr/bin' else appPath.replace('/bin', '/debian/extras/usr')
        self.setWindowIcon(QIcon(os.path.join(self.iconBase, self.iconPath)))
        self.setWindowTitle('QtPlasmaC Material File Creator')
        self.create_widgets()
        self.inButton.setEnabled(False)
        self.inFile.setEnabled(False)
        self.outUnits.hide()
        self.outMetric.hide()
        self.outImperial.hide()
        self.inButton.pressed.connect(self.on_in_button_pressed)
        self.outButton.pressed.connect(self.on_out_button_pressed)
        self.unitsGroup.buttonClicked.connect(self.units_group_clicked)
        self.modeGroup.buttonClicked.connect(self.mode_group_clicked)
        self.convert.pressed.connect(self.on_convert_clicked)
        self.cancel.pressed.connect(self.on_cancel_clicked)
        self.divisor = 1
        self.precision = 2
        self.inFileName = ''
        self.outFileName = ''
        self.fNUM = '1'
        self.fNAM = '0'
        self.fKW = '0'
        self.fPH = '0'
        self.fPD = '0'
        self.fPJH = '0'
        self.fPJD = '0'
        self.fCH = '0'
        self.fCS = '0'
        self.fCA = '0'
        self.fCV = '0'
        self.fPE = '0'
        self.fGP = '0'
        self.fCM = '1'
        wid.setStyleSheet('* {color: #ffee06; background: #16160e; font: 12pt DejaVuSans} \
                          QLabel {height: 20; width: 120} \
                          QPushButton {border: 1px solid #ffee06; border-radius: 4; height: 20; width: 120} \
                          QPushButton:disabled {color: #16160e; border: none} \
                          QPushButton:pressed {color: #16160e; background: #ffee06} \
                          QLineEdit {border: 1px solid #ffee06; border-radius: 4; height: 20; width: 360} \
                          QLineEdit:disabled {color: #16160e; border: none} \
                          QRadioButton::indicator {border: 1px solid #ffee06; border-radius: 4; height: 20; width: 20} \
                          QRadioButton::indicator:checked {background: #ffee06}'
                          )

    def create_widgets(self):
        self.T = QGridLayout()
        self.layout.addLayout(self.T)
        self.modeGroup = QButtonGroup()
        self.inManual = QRadioButton('Manual')
        self.inManual.setChecked(True)
        self.T.addWidget(self.inManual, 0, 0, 1, 1)
        self.modeGroup.addButton(self.inManual)
        self.inSheetcam = QRadioButton('SheetCam')
        self.T.addWidget(self.inSheetcam, 1, 0, 1, 1)
        self.modeGroup.addButton(self.inSheetcam)
        self.inFusion = QRadioButton('Fusion 360')
        self.T.addWidget(self.inFusion, 2, 0, 1, 1)
        self.modeGroup.addButton(self.inFusion)
        self.outUnits = QLabel('OUTPUT UNITS:')
        self.T.addWidget(self.outUnits, 0, 2, 1, 1)
        self.unitsGroup = QButtonGroup()
        self.outMetric = QRadioButton('METRIC')
        self.outMetric.setChecked(True)
        self.T.addWidget(self.outMetric, 0, 3, 1, 1)
        self.unitsGroup.addButton(self.outMetric)
        self.outImperial = QRadioButton('IMPERIAL')
        self.T.addWidget(self.outImperial, 1, 3, 1, 1)
        self.unitsGroup.addButton(self.outImperial)
        v2 = QLabel('')
        self.T.addWidget(v2, 3, 0)
        self.inButton = QPushButton('INPUT:')
        self.T.addWidget(self.inButton, 4, 0, 1, 1)
        self.inFile = QLineEdit()
        self.T.addWidget(self.inFile, 4, 1, 1, 3)
        self.outButton = QPushButton('OUTPUT:')
        self.T.addWidget(self.outButton, 5, 0, 1, 1)
        self.outFile = QLineEdit()
        self.T.addWidget(self.outFile, 5, 1, 1, 3)
        self.msgLabel = QLabel('')
        self.msgLabel.setAlignment(Qt.AlignCenter)
        self.T.addWidget(self.msgLabel, 6, 0, 1, 4)
        self.convert = QPushButton('CREATE')
        self.convert.setFixedWidth(120)
        self.T.addWidget(self.convert, 7, 0, 1, 1)
        h1 = QLabel('')
        self.T.addWidget(h1, 7, 1, 1, 1)
        h2 = QLabel('')
        self.T.addWidget(h2, 7, 2, 1, 1)
        h3 = QLabel('')
        self.T.addWidget(h3, 7, 3, 1, 1)
        self.cancel = QPushButton('EXIT')
        self.T.addWidget(self.cancel, 7, 3, 1, 1)

    def on_in_button_pressed(self):
        self.msgLabel.setText('')
        if self.inSheetcam.isChecked():
            capt = 'Select SheetCam Tool File'
            filt = 'Tool Files (*.[Tt][Oo][Oo][Ll][Ss]);;All Files (*)'
        elif self.inFusion.isChecked():
            capt = 'Select Fusion360 Tool File'
            filt = 'Tool Files (*.[Jj][Ss][Oo][Nn]);;All Files (*)'
#        name, _ = QFileDialog.getOpenFileName(self, capt, '~/', filt, filt, QFileDialog.DontUseNativeDialog)
        name, _ = QFileDialog.getOpenFileName(self, capt, '~/', filt, filt)
        if name:
            self.inFile.setText(name)
            self.inFileName = name
        else:
            self.inFile.setText('')
            self.inFileName = ''

    def on_out_button_pressed(self):
        self.msgLabel.setText('')
        capt = 'QtPlasmaC Material File'
        filt = 'Material Files (*.cfg);;All Files (*)'
#        name, _ = QFileDialog.getSaveFileName(self, capt, '~/', filt, filt, QFileDialog.DontUseNativeDialog)
        name, _ = QFileDialog.getSaveFileName(self, capt, '~/', filt, filt)
        if name:
            self.outFile.setText(name)
            self.outFileName = name
        else:
            self.outFile.setText('')
            self.outFileName = ''

    def units_group_clicked(self, button):
        self.msgLabel.setText('')
        if button.isChecked():
            if button.text() == 'Metric':
                self.divisor = 1
                self.precision = 2
            else:
                self.divisor = 25.4
                self.precision = 3

    def mode_group_clicked(self, button):
        self.msgLabel.setText('')
        if button.isChecked():
            if button.text() == 'Manual':
                self.outUnits.hide()
                self.outMetric.hide()
                self.outImperial.hide()
                self.inButton.setEnabled(False)
                self.inFile.setEnabled(False)
                self.convert.setText('CREATE')
            elif button.text() == 'SheetCam':
                self.outUnits.show()
                self.outMetric.show()
                self.outImperial.show()
                self.inButton.setEnabled(True)
                self.inFile.setEnabled(True)
                self.convert.setText('CONVERT')
            elif button.text() == 'Fusion 360':
                self.outUnits.hide()
                self.outMetric.hide()
                self.outImperial.hide()
                self.inButton.setEnabled(True)
                self.inFile.setEnabled(True)
                self.convert.setText('CONVERT')

    def on_convert_clicked(self):
        self.msgLabel.setText('')
        if self.convert.text() != 'ADD':
            if not self.inManual.isChecked():
                if not self.inFileName:
                    self.msgLabel.setText('missing input filename')
                    return
                if not os.path.exists(self.inFileName):
                    self.msgLabel.setText(f'{self.inFileName} missing')
                    return
            if not self.outFileName:
                self.msgLabel.setText('missing output filename')
                return
            self.msgLabel.setText('converting...')
#            try:
            with open(self.outFileName, 'w') as f_out:
                f_out.write('#plasmac material file\n'
                            '# example only, may be deleted\n'
                            '# items marked * are mandatory\n'
                            '# other items are optional and will default to 0\n'
                            '#[MATERIAL_NUMBER_1]  \n'
                            '#NAME               = \n'
                            '#KERF_WIDTH         = \n'
                            '#PIERCE_HEIGHT      = *\n'
                            '#PIERCE_DELAY       = *\n'
                            '#PUDDLE_JUMP_HEIGHT = \n'
                            '#PUDDLE_JUMP_DELAY  = \n'
                            '#CUT_HEIGHT         = *\n'
                            '#CUT_SPEED          = *\n'
                            '#CUT_AMPS           = \n'
                            '#CUT_VOLTS          = \n'
                            '#PAUSE_AT_END       = \n'
                            '#GAS_PRESSURE       = \n'
                            '#CUT_MODE           = \n'
                            '\n')
#            except:
#                self.msgLabel.setText('WRITE ERROR!!!')
        else:
            self.fNUM = str(int(self.fNUM) + 1)
        if self.inManual.isChecked():
            getParams = self.fusion_dialog()
            if not getParams:
                self.msgLabel.setText('')
                return
            self.materialNum = f'[MATERIAL_NUMBER_{self.fNUM}]'
            self.materialName = f'NAME               = {self.fNAM}'
            self.materialKerf = f'KERF_WIDTH         = {float(self.fKW) / self.divisor:.{self.precision}f}'
            self.materialPierceH = f'PIERCE_HEIGHT      = {float(self.fPH) / self.divisor:.{self.precision}f}'
            self.materialPierceD = f'PIERCE_DELAY       = {self.fPD}'
            self.materialPuddleH = f'PUDDLE_JUMP_HEIGHT = {float(self.fPJH) / self.divisor:.{self.precision}f}'
            self.materialPuddleD = f'PUDDLE_JUMP_DELAY  = {self.fPJD}'
            self.materialCutH = f'CUT_HEIGHT         = {float(self.fCH) / self.divisor:.{self.precision}f}'
            self.materialCutS = f'CUT_SPEED          = {float(self.fCS) / self.divisor:.0f}'
            self.materialCutA = f'CUT_AMPS           = {self.fCA}'
            self.materialCutV = f'CUT_VOLTS          = {self.fCV}'
            self.materialPauseE = f'PAUSE_AT_END       = {self.fPE}'
            self.materialGasP = f'GAS_PRESSURE       = {self.fGP}'
            self.materialCutM = f'CUT_MODE           = {self.fCM}'
            self.output()
            self.convert.setText('ADD')
        elif self.inSheetcam.isChecked():
            # try:
            with open(self.inFileName, 'r') as f_in:
                count = 0
                valid = False
                for line in f_in:
                    if line.startswith('[Tool') and 'Custom' not in line:
                        if count and valid:
                            self.output()
                        valid = False
                        self.materialNum = ''
                        self.materialName = 'NAME               = '
                        self.materialKerf = 'KERF_WIDTH         = '
                        self.materialPierceH = 'PIERCE_HEIGHT      = '
                        self.materialPierceD = 'PIERCE_DELAY       = '
                        self.materialPuddleH = 'PUDDLE_JUMP_HEIGHT = 0'
                        self.materialPuddleD = 'PUDDLE_JUMP_DELAY  = 0'
                        self.materialCutH = 'CUT_HEIGHT         = '
                        self.materialCutS = 'CUT_SPEED          = '
                        self.materialCutA = 'CUT_AMPS           = 0'
                        self.materialCutV = 'CUT_VOLTS          = 0'
                        self.materialPauseE = 'PAUSE_AT_END       = 0'
                        self.materialGasP = 'GAS_PRESSURE       = 0'
                        self.materialCutM = 'CUT_MODE           = 1'
                    elif 'PlasmaTool' in line:
                        valid = True
                    elif line.startswith(r'Tool\ number'):
                        b = line.split('=')[1]
                        self.materialNum = f'[MATERIAL_NUMBER_{b.strip().replace("]","")}]'
                    elif line.startswith('Name='):
                        b = line.split('=', 1)[1]
                        self.materialName = f'NAME               = {b.strip()}'
                    elif line.startswith(r'Kerf\ width'):
                        b = line.split('=', 1)[1]
                        self.materialKerf = f'KERF_WIDTH         = {float(b.strip()) / self.divisor:.{self.precision}f}'
                    elif line.startswith(r'Pierce\ height'):
                        b = line.split('=', 1)[1]
                        self.materialPierceH = f'PIERCE_HEIGHT      = {float(b.strip()) / self.divisor:.{self.precision}f}'
                    elif line.startswith(r'Pierce\ delay'):
                        b = line.split('=', 1)[1]
                        self.materialPierceD = f'PIERCE_DELAY       = {b.strip()}'
                    elif line.startswith(r'Cut\ height'):
                        b = line.split('=', 1)[1]
                        self.materialCutH = f'CUT_HEIGHT         = {float(b.strip()) / self.divisor:.{self.precision}f}'
                    elif line.startswith(r'Feed\ rate'):
                        b = line.split('=', 1)[1]
                        self.materialCutS = f'CUT_SPEED          = {float(b.strip()) / self.divisor:.0f}'
                    elif line.startswith(r'Cut\ current') or (line.startswith(r'Preset\ current') and self.materialCutA == 'CUT_AMPS           = 0'):
                        b = line.split('=', 1)[1]
                        self.materialCutA = f'CUT_AMPS           = {b.strip()}'
                    elif line.startswith(r'Cut\ voltage') or (line.startswith(r'Preset\ volts') and self.materialCutV == 'CUT_VOLTS          = 0'):
                        b = line.split('=', 1)[1]
                        self.materialCutV = f'CUT_VOLTS          = {b.strip()}'
                    elif line.startswith(r'Pause\ at\ end\ of\ cut'):
                        b = line.split('=', 1)[1]
                        self.materialPauseE = f'PAUSE_AT_END       = {b.strip()}'
                    elif line.startswith(r'Gas\ pressure') or (line.startswith(r'Preset\ Air\ Pressure') and self.materialGasP == 'GAS_PRESSURE       = 0'):
                        b = line.split('=', 1)[1]
                        self.materialGasp = f'GAS_PRESSURE       = {b.strip()}'
                    elif line.startswith(r'Cut\ mode') or (line.startswith(r'Preset\ mode') and self.materialCutM == 'CUT_MODE           = 1'):
                        b = line.split('=', 1)[1]
                        self.materialCutM = f'CUT_MODE           = {b.strip()}'
                    count += 1
                if valid:
                    self.output()
#            except:
#                self.msgLabel.setText('READ ERROR!!!')
            self.msgLabel.setText('FINISHED')
        elif self.inFusion.isChecked():
            import json
            #            try:
            with open(self.inFileName, 'r') as f_in:
                jdata = json.load(f_in)
                f_in.close()
            for t in jdata['data']:
                self.fNUM = str(t['post-process']['number'])
                self.fNAM = t['description']
                lCS = self.find_key(t, 'v_f')
                lKW = t['geometry']['CW']
                getParams = self.fusion_dialog()
                if not getParams:
                    self.msgLabel.setText('')
                    return
                self.materialNum = f'[MATERIAL_NUMBER_{self.fNUM}]'
                self.materialName = f'NAME               = {self.fNAM}'
                self.materialKerf = f'KERF_WIDTH         = {float(lKW) / self.divisor:.{self.precision}f}'
                self.materialPierceH = f'PIERCE_HEIGHT      = {float(self.fPH) / self.divisor:.{self.precision}f}'
                self.materialPierceD = f'PIERCE_DELAY       = {self.fPD}'
                self.materialPuddleH = f'PUDDLE_JUMP_HEIGHT = {float(self.fPJH) / self.divisor:.{self.precision}f}'
                self.materialPuddleD = f'PUDDLE_JUMP_DELAY  = {self.fPJD}'
                self.materialCutH = f'CUT_HEIGHT         = {float(self.fCH) / self.divisor:.{self.precision}f}'
                self.materialCutS = f'CUT_SPEED          = {float(lCS) / self.divisor:.0f}'
                self.materialCutA = f'CUT_AMPS           = {self.fCA}'
                self.materialCutV = f'CUT_VOLTS          = {self.fCV}'
                self.materialPauseE = f'PAUSE_AT_END       = {self.fPE}'
                self.materialGasP = f'GAS_PRESSURE       = {self.fGP}'
                self.materialCutM = f'CUT_MODE           = {self.fCM}'
                self.output()
#            except:
#                self.msgLabel.setText('READ ERROR!!!')
            self.msgLabel.setText('FINISHED')
        else:
            self.msgLabel.setText('Invalid Conversion')

    def find_key(self, obj, key):
        if key in obj:
            return obj[key]
        for k, v in obj.items():
            # variable is not currently used
            del k
            if isinstance(v, dict):
                item = self.find_key(v, key)
                if item is not None:
                    return item
            elif isinstance(v, list):
                for list_item in v:
                    item = self.find_key(list_item, key)
                    if item is not None:
                        return item

    def on_cancel_clicked(self):
        sys.exit()

    def output(self):
        # try:
        with open(self.outFileName, 'a') as f_out:
            f_out.write(self.materialNum + '\n' +
                        self.materialName + '\n' +
                        self.materialKerf + '\n' +
                        self.materialPierceH + '\n' +
                        self.materialPierceD + '\n' +
                        self.materialPuddleH + '\n' +
                        self.materialPuddleD + '\n' +
                        self.materialCutH + '\n' +
                        self.materialCutS + '\n' +
                        self.materialCutA + '\n' +
                        self.materialCutV + '\n' +
                        self.materialPauseE + '\n' +
                        self.materialGasP + '\n' +
                        self.materialCutM + '\n' +
                        '\n')
#        except:
#            self.msgLabel.setText('WRITE ERROR!!!')

    def fusion_dialog(self):
        dialog = QDialog()
        dialog.setWindowTitle('Material Maker')
        dialog.setWindowIcon(QIcon(os.path.join(self.iconBase, self.iconPath)))
        dialog.setModal(True)
        topL = QLabel('Items with a *** are mandatory')
        infL = QLabel(f'For Material # {self.fNUM}\n{self.fNAM}')
        dNUl = QLabel('Material Number ***')
        dNUl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dNU = QLineEdit()
        dNU.setText(self.fNUM)
        dNU.setAlignment(Qt.AlignRight)
        dNAl = QLabel('Material Name')
        dNAl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dNA = QLineEdit()
        dNA.setText(self.fNAM)
        dNA.setAlignment(Qt.AlignRight)
        dKWl = QLabel('Kerf Width')
        dKWl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dKW = QLineEdit()
        dKW.setText(self.fKW)
        dKW.setAlignment(Qt.AlignRight)
        dPHl = QLabel('Pierce Height ***')
        dPHl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dPH = QLineEdit()
        dPH.setText(self.fPH)
        dPH.setAlignment(Qt.AlignRight)
        dPDl = QLabel('Pierce Delay ***')
        dPDl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dPD = QLineEdit()
        dPD.setText(self.fPD)
        dPD.setAlignment(Qt.AlignRight)
        dPJHl = QLabel('Puddle Jump Height')
        dPJHl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dPJH = QLineEdit()
        dPJH.setText(self.fPJH)
        dPJH.setAlignment(Qt.AlignRight)
        dPJDl = QLabel('Puddle Jump Delay')
        dPJDl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dPJD = QLineEdit()
        dPJD.setText(self.fPJD)
        dPJD.setAlignment(Qt.AlignRight)
        dCHl = QLabel('Cut Height ***')
        dCHl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dCH = QLineEdit()
        dCH.setText(self.fCH)
        dCH.setAlignment(Qt.AlignRight)
        dCSl = QLabel('Cut Speed ***')
        dCSl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dCS = QLineEdit()
        dCS.setText(self.fCS)
        dCS.setAlignment(Qt.AlignRight)
        dCAl = QLabel('Cut Amps')
        dCAl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dCA = QLineEdit()
        dCA.setText(self.fCA)
        dCA.setAlignment(Qt.AlignRight)
        dCVl = QLabel('Cut Volts')
        dCVl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dCV = QLineEdit()
        dCV.setText(self.fCV)
        dCV.setAlignment(Qt.AlignRight)
        dPEl = QLabel('Pause At End Of Cut')
        dPEl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dPE = QLineEdit()
        dPE.setText(self.fPE)
        dPE.setAlignment(Qt.AlignRight)
        dGPl = QLabel('Gas Pressure')
        dGPl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dGP = QLineEdit()
        dGP.setText(self.fGP)
        dGP.setAlignment(Qt.AlignRight)
        dCMl = QLabel('Cut Mode')
        dCMl.setAlignment(Qt.AlignBottom | Qt.AlignRight)
        dCM = QLineEdit()
        dCM.setText(self.fCM)
        dCM.setAlignment(Qt.AlignRight)
        vSpace1 = QSpacerItem(0, 25)
        buttons = QDialogButtonBox.Ok | QDialogButtonBox.Cancel
        buttonBox = QDialogButtonBox(buttons)
        buttonBox.accepted.connect(dialog.accept)
        buttonBox.rejected.connect(dialog.reject)
        layout = QVBoxLayout()
        dialog.setLayout(layout)
        layout.addWidget(topL)
        if self.inManual.isChecked():
            layout.addWidget(dNUl)
            layout.addWidget(dNU)
            layout.addWidget(dNAl)
            layout.addWidget(dNA)
            layout.addWidget(dKWl)
            layout.addWidget(dKW)
        else:
            layout.addWidget(infL)
        layout.addWidget(dPHl)
        layout.addWidget(dPH)
        layout.addWidget(dPDl)
        layout.addWidget(dPD)
        layout.addWidget(dPJHl)
        layout.addWidget(dPJH)
        layout.addWidget(dPJDl)
        layout.addWidget(dPJD)
        layout.addWidget(dCHl)
        layout.addWidget(dCH)
        if self.inManual.isChecked():
            layout.addWidget(dCSl)
            layout.addWidget(dCS)
        layout.addWidget(dCAl)
        layout.addWidget(dCA)
        layout.addWidget(dCVl)
        layout.addWidget(dCV)
        layout.addWidget(dPEl)
        layout.addWidget(dPE)
        layout.addWidget(dGPl)
        layout.addWidget(dGP)
        layout.addWidget(dCMl)
        layout.addWidget(dCM)
        layout.addItem(vSpace1)
        layout.addWidget(buttonBox, alignment=Qt.AlignCenter)
        dialog.setStyleSheet('* { color: #ffee06; background: #16160e; font: 10pt DejaVuSans } \
                             QLineEdit { border: 1px solid #ffee06; border-radius: 4 } \
                           QPushButton {border: 1px solid #ffee06; border-radius: 4; height: 20; width: 80} \
                           QPushButton:pressed {color: #16160e; background: #ffee06}')
        response = dialog.exec_()
        if self.inManual.isChecked():
            self.fNUM = dNU.text()
            self.fNAM = dNA.text()
            self.fKW = dKW.text()
        self.fPH = dPH.text()
        self.fPD = dPD.text()
        self.fPJH = dPJH.text()
        self.fPJD = dPJD.text()
        self.fCH = dCH.text()
        if self.inManual.isChecked():
            self.fCS = dCS.text()
        self.fCA = dCA.text()
        self.fCV = dCV.text()
        self.fPE = dPE.text()
        self.fGP = dGP.text()
        self.fCM = dCM.text()
        return response


if __name__ == '__main__':
    app = QApplication(sys.argv)
    w = MaterialConverter()
    w.show()
    sys.exit(app.exec_())
