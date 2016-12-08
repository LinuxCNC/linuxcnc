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
from qtvcp.plugins.simplewidgetsplugin import *
from qtvcp.plugins.ledplugin import LedPlugin

# Linuxcnc widgets
from qtvcp.plugins.container_plug import StateEnableGridLayoutPlugin
from qtvcp.plugins.graphicsplugin import *
from qtvcp.plugins.lcnc_widgetplugin import *
from qtvcp.plugins.ledstateplugin import LedStatePlugin
from qtvcp.plugins.gstat_label_plugin import GstatLabelPlugin
from qtvcp.plugins.gstat_bool_label_plugin import GstatBoolLabelPlugin
from qtvcp.plugins.actionbutton_plugin import ActionButtonPlugin
from qtvcp.plugins.dialog_plugin import *
from qtvcp.plugins.overlayplugin import FocusOverlayPlugin
from qtvcp.plugins.gstat_slider_plugin import GstatSliderPlugin
from qtvcp.plugins.screenoptionsplugin import LcncScreenOptionsPlugin
from qtvcp.plugins.jogincrementsplugin import JogIncrementsPlugin
