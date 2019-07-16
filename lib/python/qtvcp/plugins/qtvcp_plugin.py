#!/usr/bin/python

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

# HAL only widgets
from qtvcp.plugins.simplewidgets_plugin import *
from qtvcp.plugins.led_plugin import LEDPlugin

# Linuxcnc widgets
from qtvcp.plugins.container_plugin import StateEnableGridLayoutPlugin
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
from qtvcp.plugins.toolbutton_plugin import SystemToolButtonPlugin
from qtvcp.plugins.versa_probe_plugin import VersaProbePlugin
