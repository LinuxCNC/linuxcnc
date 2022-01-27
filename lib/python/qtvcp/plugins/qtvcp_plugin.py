#!/usr/bin/env python3

#    QT Designer custom widget plugin imports for linuxcnc
#
#    Chris Morley copyright 2012
#
#    This program is free software; you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation; either version 2 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program; if not, write to the Free Software
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

print('Qtvcp python plugin found:',__file__)
# HAL only widgets
from qtvcp.plugins.simplewidgets_plugin import *
from qtvcp.plugins.led_plugin import LEDPlugin
from qtvcp.plugins.hal_label_plugin import HALLabelPlugin
from qtvcp.plugins.detach_tabs_plugin import DetachTabWidgetPlugin
from qtvcp.plugins.round_progress_bar_plugin import RoundProgressBarPlugin
from PyQt5.QtCore import PYQT_VERSION_STR
try:
    v = PYQT_VERSION_STR.split('.')
    if int(v[1]) > 10:
        from qtvcp.plugins.joypad_plugin import *
except:
    print('PyQt version {} to old for JoyPad widget'.format(PYQT_VERSION_STR))

# plain widgets
from qtvcp.plugins.nurbs_editor_plugin import NurbsEditorPlugin

# Linuxcnc widgets
from qtvcp.plugins.container_plugin import StateEnableGridLayoutPlugin
from qtvcp.plugins.container_plugin import JointEnableWidgetPlugin
from qtvcp.plugins.graphics_plugin import GCodeGraphicsPlugin
from qtvcp.plugins.widgets_plugin import *
from qtvcp.plugins.state_led_plugin import StateLEDPlugin
from qtvcp.plugins.status_label_plugin import StatusLabelPlugin
from qtvcp.plugins.state_label_plugin import StateLabelPlugin
from qtvcp.plugins.actionbutton_plugin import *
from qtvcp.plugins.dialog_plugin import *
from qtvcp.plugins.overlay_plugin import FocusOverlayPlugin
from qtvcp.plugins.status_slider_plugin import StatusSliderPlugin
from qtvcp.plugins.status_adjustment_bar_plugin import StatusAdjustmentBarPlugin
from qtvcp.plugins.screenoptions_plugin import LcncScreenOptionsPlugin
from qtvcp.plugins.jogincrements_plugin import JogIncrementsPlugin
from qtvcp.plugins.camview_plugin import CamViewPlugin
from qtvcp.plugins.toolbutton_plugin import *
from qtvcp.plugins.versa_probe_plugin import VersaProbePlugin
from qtvcp.plugins.basic_probe_plugin import BasicProbePlugin
from qtvcp.plugins.tab_widget_plugin import TabWidgetPlugin
from qtvcp.plugins.virtualkeyboard_plugin import VirtualKeyboardPlugin
from qtvcp.plugins.round_gauge_plugin import GaugePlugin
