#!/usr/bin/env python

from PyQt4 import QtCore, QtGui
from PyQt4.QtDesigner import QPyDesignerCustomWidgetPlugin
from qtvcp_widgets.graphics import Graphics


class GraphicsPlugin(QPyDesignerCustomWidgetPlugin):

    # The __init__() method is only used to set up the plugin and define its
    # initialized variable.
    def __init__(self, parent = None):
    
        QPyDesignerCustomWidgetPlugin.__init__(self)
        self.initialized = False

    # The initialize() and isInitialized() methods allow the plugin to set up
    # any required resources, ensuring that this can only happen once for each
    # plugin.
    def initialize(self, formEditor):

        if self.initialized:
            return
        
        self.initialized = True

    def isInitialized(self):
        return self.initialized

    # This factory method creates new instances of our custom widget with the
    # appropriate parent.
    def createWidget(self, parent):
        return Graphics(parent)

    # This method returns the name of the custom widget class that is provided
    # by this plugin.
    def name(self):
        return "Graphics"

    # Returns the name of the group in Qt Designer's widget box that this
    # widget belongs to.
    def group(self):
        return "Linuxcnc"

    # Returns the icon used to represent the custom widget in Qt Designer's
    # widget box.
    def icon(self):
        return QtGui.QIcon(_logo_pixmap)

    # Returns a short description of the custom widget for use in a tool tip.
    def toolTip(self):
        return "3D grahics display widget"

    # Returns a short description of the custom widget for use in a "What's
    # This?" help message for the widget.
    def whatsThis(self):
        return ""

    # Returns True if the custom widget acts as a container for other widgets;
    # otherwise returns False. Note that plugins for custom containers also
    # need to provide an implementation of the QDesignerContainerExtension
    # interface if they need to add custom editing support to Qt Designer.
    def isContainer(self):
        return False

    # Returns an XML description of a custom widget instance that describes
    # default values for its properties. Each custom widget created by this
    # plugin will be configured using this description.
    def domXml(self):
        return '<widget class="Graphics" name="graphics" />\n'

    # Returns the module containing the custom widget class. It may include
    # a module path.
    def includeFile(self):
        return "linuxcnc_qt_widgets.graphics"


# Define the image used for the icon.
_logo_32x32_xpm = [
    "32 32 118 2",
    "AB c #010101", "AD c #030303", "AE c #040404", "AH c #070707",
    "AI c #080808", "AJ c #090909", "AN c #0d0d0d", "AO c #0e0e0e",
    "AP c #0f0f0f", "AQ c #101010", "AR c #111111", "AS c #121212",
    "AT c #131313", "AU c #141414", "AV c #151515", "AX c #171717",
    "AY c #181818", "AZ c #191919", "BA c #1a1a1a", "BB c #1b1b1b",
    "BC c #1c1c1c", "BD c #1d1d1d", "BE c #1e1e1e", "BF c #1f1f1f",
    "BK c #242424", "BL c #252525", "BM c #262626", "BN c #272727",
    "BO c #282828", "BU c #2e2e2e", "BW c #303030", "BX c #313131",
    "BZ c #333333", "CF c #393939", "CI c #3c3c3c", "CK c #3e3e3e",
    "CL c #3f3f3f", "CM c #404040", "CN c #414141", "CO c #424242",
    "CP c #434343", "CR c #454545", "DG c #545454", "DH c #555555",
    "DI c #565656", "DJ c #575757", "DK c #585858", "DN c #5b5b5b",
    "DO c #5c5c5c", "DP c #5d5d5d", "DQ c #5e5e5e", "DR c #5f5f5f",
    "DS c #606060", "DT c #616161", "DU c #626262", "DY c #666666",
    "EA c #686868", "ED c #6b6b6b", "EE c #6c6c6c", "EI c #707070",
    "EL c #737373", "EQ c #787878", "ET c #7b7b7b", "EU c #7c7c7c",
    "FA c #828282", "FB c #838383", "FD c #858585", "FF c #878787",
    "FG c #888888", "FI c #8a8a8a", "FK c #8c8c8c", "FL c #8d8d8d",
    "FO c #909090", "FS c #949494", "FY c #9a9a9a", "FZ c #9b9b9b",
    "GB c #9d9d9d", "GC c #9e9e9e", "GF c #a1a1a1", "GH c #a3a3a3",
    "GR c #adadad", "GU c #b0b0b0", "GV c #b1b1b1", "GY c #b4b4b4",
    "GZ c #b5b5b5", "HB c #b7b7b7", "HC c #b8b8b8", "HE c #bababa",
    "HL c #c1c1c1", "HO c #c4c4c4", "HP c #c5c5c5", "HU c #cacaca",
    "HY c #cecece", "HZ c #cfcfcf", "IB c #d1d1d1", "IC c #d2d2d2",
    "ID c #d3d3d3", "IE c #d4d4d4", "IG c #d6d6d6", "IH c #d7d7d7",
    "II c #d8d8d8", "IJ c #d9d9d9", "IK c #dadada", "IL c #dbdbdb",
    "IM c #dcdcdc", "IO c #dedede", "IS c #e2e2e2", "JB c #ebebeb",
    "JC c #ececec", "JD c #ededed", "JE c #eeeeee", "JF c #efefef",
    "JH c #f1f1f1", "JP c #f9f9f9", "JR c #fbfbfb", "JT c #fdfdfd",
    "JU c #fefefe", "JV c #ffffff",
    "JVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJV",
    "JVJVJVJVJVJVJVJVJVJVJVJVIMFDCMBLBLCNFFIOJVJVJVJVJVJVJVJVJVJVJVJV",
    "JVJVJVJVJVJVJVJVJVJDDOANABADAJAZAZAJADABAODPJFJVJVJVJVJVJVJVJVJV",
    "JVJVJVJVJVJVJVJRDUADBEDSBDHEJVJVJVJVHCBCDTBDADDYJTJVJVJVJVJVJVJV",
    "JVJVJVJVJVJVIKAVARGZHZBEILJVJVJVJVJVJVIIBFICGYAQAYIOJVJVJVJVJVJV",
    "JVJVJVJVJVHUAHCRJRJRBUHEJVJVJVJVJVJVJVJVHBBXJRJRCOAIHYJVJVJVJVJV",
    "JVJVJVJVIKAHDUJVJVFYDKJVJVJVJVJVJVJVJVJVJVDIGBJVJVDQAIIOJVJVJVJV",
    "JVJVJVJRAVCRJVJVJTBLIIJBHLGFFKFAFAFKGFHLJCIEBNJTJVJVCMAYJTJVJVJV",
    "JVJVJVDUARJRJVJTFKBBBKAOCFDOEDEQEUELDQCLAUBLAZFOJTJVJPAPEAJVJVJV",
    "JVJVJDADGZJRFYBKBADGIJJUJVJVJVJVJVJVJVJVJUILDIBBBMFZJRGUADJFJVJV",
    "JVJVDOBEHZBUDHHZBKIMJVJVJVJVJVJVJVJVJVJVJVJVIJBMIGDJBWIBBCDRJVJV",
    "JVJVANDSBDGVJVJBAUJUJVJVJVJVJVJVJVJVJVJVJVJVJUATJEJVHCBEDQAPJVJV",
    "JVIMABAZIDJVJVHLCLJVJVJVJVJVJVJVJVJVJVJVJVJVJVCIHOJVJVIJBCABISJV",
    "JVFDADGVJVJVJVGFDQJVJVJVJVJVJVJVJVJVJVJVJVJVJVDOGHJVJVJVHBADFGJV",
    "JVCMAIJUJVJVJVFKELJVJVJVJVJVJVJVJVJVJVJVJVJVJVEIFLJVJVJVJVAICPJV",
    "JVBLAXJVJVJVJVFAEUJVJVJVJVJVJVJVJVJVJVJVJVJVJVETFBJVJVJVJVAZBMJV",
    "JVBLAZJVJVJVJVFAEUJVJVJVJVJVJVJVJVJVJVJVJVJVJVETFBJVJVJVJVAZBMJV",
    "JVCNAJJVJVJVJVFKELJVJVJVJVJVJVJVJVJVJVJVJVJVJVEIFLJVJVJVJUAICPJV",
    "JVFFADHCJVJVJVGFDQJVJVJVJVJVJVJVJVJVJVJVJVJVJVDNGHJVJVJVGZADFIJV",
    "JVIOABBCIIJVJVHLCLJVJVJVJVJVJVJVJVJVJVJVJVJVJVCIHPJVJVIHBCABISJV",
    "JVJVAODTBFHBJVJCAUJUJVJVJVJVJVJVJVJVJVJVJVJVJUASJEJVGZBFDSAPJVJV",
    "JVJVDPBDICBXDIIEBLILJVJVJVJVJVJVJVJVJVJVJVJVIJBNICDGBZIEBBDTJVJV",
    "JVJVJFADGYJRGBBNAZDIIJJUJVJVJVJVJVJVJVJVJUIJDGBABOGCJRGRAEJHJVJV",
    "JVJVJVDYAQJRJVJTFOBBBMATCIDOEIETETEIDNCIATBNBAFSJTJVJPAOEEJVJVJV",
    "JVJVJVJTAYCOJVJVJTBMIGJEHOGHFLFBFBFLGHHPJEICBOJTJVJVCKBAJTJVJVJV",
    "JVJVJVJVIOAIDQJVJVFZDJJVJVJVJVJVJVJVJVJVJVDGGCJVJVDOAIISJVJVJVJV",
    "JVJVJVJVJVHYAICNJRJRBWHCJVJVJVJVJVJVJVJVGZBZJRJPCKAIHZJVJVJVJVJV",
    "JVJVJVJVJVJVIOAYAPGUIBBEIJJVJVJVJVJVJVIHBFIEGRAOBAISJVJVJVJVJVJV",
    "JVJVJVJVJVJVJVJTEAADBCDQBCHBJVJVJVJUGZBCDSBBAEEEJTJVJVJVJVJVJVJV",
    "JVJVJVJVJVJVJVJVJVJFDRAPABADAIAZAZAIADABAPDTJHJVJVJVJVJVJVJVJVJV",
    "JVJVJVJVJVJVJVJVJVJVJVJVISFGCPBMBMCPFIISJVJVJVJVJVJVJVJVJVJVJVJV",
    "JVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJVJV"]

_logo_pixmap = QtGui.QPixmap(_logo_32x32_xpm)
