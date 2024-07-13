#!/usr/bin/env python3
# Qtvcp
#
# Copyright (c) 2017  Chris Morley <chrisinnanaimo@hotmail.com>
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
#
###############################################################################

import os
import sys

from PyQt5 import QtCore

# Set up logging
from . import logger

LOG = logger.getLogger(__name__)


# Force the log level for this module
# LOG.setLevel(logger.DEBUG) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

# BASE is the absolute path to linuxcnc base
# LIBDIR is the path to qtvcp python files
# DATADIR is where the standard UI files are
# IMAGEDIR is for icons
class _PStat(object):
    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >= 1:
            return
        self.__class__._instanceNum += 1

        try:
            self.WORKINGDIR = os.getcwd()

            # widget directory
            here = os.path.dirname(os.path.realpath(__file__))
            self.LIBDIR = os.path.join(here,"lib")
            self.WIDGETDIR = os.path.join(here, "widgets")
            self.PLUGINDIR = os.path.join(here,"plugins")
            self.VISMACHDIR = os.path.join(self.LIBDIR, "qt_vismach")

            # share directory moves when using RIP vrs installed
            home = os.environ.get('EMC2_HOME', '/usr')
            if home is not None:
                self.SHAREDIR = os.path.join(home,"share", "qtvcp")
            self.IMAGEDIR = os.path.join(self.SHAREDIR,  "images")
            self.SCREENDIR = os.path.join(self.SHAREDIR, "screens")
            self.PANELDIR = os.path.join(self.SHAREDIR, "panels")
            self.WIDGETUI = os.path.join(self.SHAREDIR, "widgets_ui")

            # Linuxcnc project base directory moves when using RIP vrs installed
            try:
                self.BASEDIR = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
            except:
                # TODO not sure if working directory is right - this is to fix an error with designer
                # loading widget libraries
                self.BASEDIR = self.WORKINGDIR
            self.RIPCONFIGDIR = os.path.join(self.BASEDIR, "configs", "sim", "qtvcp_screens")

            # python RIP library directory
            self.PYDIR = os.path.join(self.BASEDIR, "lib", "python")
            sys.path.insert(0, self.PYDIR)

        except Exception as e:
            print ('qt_Pstat:',e)
            pass

    def set_paths(self, filename='dummy', isscreen=False):
        self.PREFS_FILENAME = None
        self.IS_SCREEN = isscreen
        self.QRC_IS_LOCAL = None
        self.QRCPY_IS_LOCAL = None

        if isscreen:
            # path to the configuration the user requested
            self.CONFIGPATH = os.environ['CONFIG_DIR']
            sys.path.insert(0, self.CONFIGPATH)
        else:
            # VCP panels don't usually have config paths but QTVCP looks for one.
            # TODO this fixes the error but maybe it should be something else
            self.CONFIGPATH = self.WORKINGDIR

        # record the original argument passed to us
        self.ARGUMENT = filename
        # PyQt's .ui file's basename
        self.BASENAME = os.path.splitext(os.path.basename(filename))[0]
        # base path (includes any extra path commands
        self.BASEPATH = os.path.splitext(filename)[0]
        LOG.debug('Passed name={}, BASENAME={} BASEPATH={}'.format(self.ARGUMENT, self.BASENAME, self.BASEPATH))
        LOG.verbose('Working directory: {}'.format(self.WORKINGDIR))

        # look for custom handler files:
        handler_fn = "{}_handler.py".format(self.BASEPATH)
        local = []
        if self.IS_SCREEN:
            # builtin screen folder
            default_handler_path = os.path.join(self.SCREENDIR, self.BASEPATH, handler_fn)
            # relative to configuration folder
            local.append( os.path.join(self.CONFIGPATH, handler_fn))
            # in standard folder
            local.append( os.path.join(self.CONFIGPATH, 'qtvcp/screens',self.BASEPATH, handler_fn))
            # legacy standard folder
            local.append( os.path.join(self.CONFIGPATH, self.BASEPATH, handler_fn))
        else:
            # in standard folder
            local.append( os.path.join(self.WORKINGDIR, 'qtvcp/panels',self.BASEPATH, handler_fn))
            # relative to configuration folder
            local.append( os.path.join(self.WORKINGDIR, handler_fn))
            # builtin panel folder
            default_handler_path = os.path.join(self.PANELDIR, self.BASEPATH, handler_fn)

        for local_handler_path in local:
            LOG.debug("Checking for handler file in: yellow<{}>".format(local_handler_path))
            if os.path.exists(local_handler_path):
                self.HANDLER = local_handler_path
                LOG.info("Using LOCAL handler file path: yellow<{}>".format(self.HANDLER))
                break
        # if no break
        else:
            LOG.debug("Checking for default handler file in: yellow<{}>".format(default_handler_path))
            if os.path.exists(default_handler_path):
                self.HANDLER = default_handler_path
                LOG.info("Using DEFAULT handler file path: yellow<{}>".format(self.HANDLER))
            else:
                self.HANDLER = None
                LOG.info("No handler file found.")
        if not self.HANDLER is None:
            self.HANDLERDIR = os.path.dirname(self.HANDLER)
        else:
            self.HANDLERDIR = None

        # look for custom ui file
        ui_fn = "{}.ui".format(self.BASEPATH)
        local = []
        if self.IS_SCREEN:
            defaultui = os.path.join(self.SCREENDIR, self.BASEPATH, ui_fn)
            local.append( os.path.join(self.CONFIGPATH, ui_fn))
            local.append( os.path.join(self.CONFIGPATH, 'qtvcp/screens',self.BASEPATH, ui_fn))
            local.append( os.path.join(self.CONFIGPATH,self.BASEPATH, ui_fn))
        else:
            local.append( os.path.join(self.WORKINGDIR, 'qtvcp/panels',self.BASEPATH, ui_fn))
            local.append( os.path.join(self.WORKINGDIR, ui_fn))
            defaultui = os.path.join(self.PANELDIR, self.BASEPATH, ui_fn)

        for localui in local:
            LOG.debug("Checking for .ui in: yellow<{}>".format(localui))
            if os.path.exists(localui):
                LOG.info("Using LOCAL ui file from: yellow<{}>".format(localui))
                self.XML = localui
                break
        # if no break
        else:
            LOG.debug("Checking for .ui in: yellow<{}>".format(defaultui))
            if os.path.exists(defaultui):
                LOG.info("Using DEFAULT ui file from: yellow<{}>".format(defaultui))
                self.XML = defaultui
            else:
                # error
                self.XML = None
                LOG.critical("No UI file found - Did you add the .ui name/path?")
                LOG.info('Available built-in Machine Control Screens:')
                for i in self.find_screen_dirs():
                   print(('{}'.format(i)))
                print('')
                LOG.info('Available built-in VCP Panels:')
                for i in self.find_panel_dirs():
                    print(('{}'.format(i)))
                print('')
                return True # error

        if not self.HANDLER is None:
            self.XMLDIR = os.path.dirname(self.HANDLER)
        else:
            self.XMLDIR = None

        # check for qss file
        qss_fn = "{}.qss".format(self.BASEPATH)
        local = []
        if self.IS_SCREEN:
            defaultqss = os.path.join(self.SCREENDIR, self.BASEPATH, qss_fn)
            local.append( os.path.join(self.CONFIGPATH, qss_fn))
            local.append( os.path.join(self.CONFIGPATH, 'qtvcp/screens',self.BASEPATH, qss_fn))
            local.append( os.path.join(self.CONFIGPATH, self.BASEPATH, qss_fn))
        else:
            local.append( os.path.join(self.WORKINGDIR, 'qtvcp/panels',self.BASEPATH, qss_fn))
            local.append( os.path.join(self.WORKINGDIR, qss_fn))
            defaultqss = os.path.join(self.PANELDIR, self.BASEPATH, qss_fn)

        for localqss in local:
            LOG.debug("Checking for .qss in: yellow<{}>".format(localqss))
            if os.path.exists(localqss):
                LOG.info("Using LOCAL qss file from: yellow<{}>".format(localqss))
                self.QSS = localqss
                break
        # if no break
        else:
            LOG.debug("Checking for .qss in: yellow<{}>".format(defaultqss))
            if os.path.exists(defaultqss):
                LOG.debug("Using DEFAULT qss file from: yellow<{}>".format(defaultqss))
                self.QSS = defaultqss
            else:
                self.QSS = None
                LOG.info("No qss file found.")

        # check for qrc file
        qrc_fn = "{}.qrc".format(self.BASEPATH)
        local = []
        if self.IS_SCREEN:
            defaultqrc = os.path.join(self.SCREENDIR, self.BASEPATH, qrc_fn)
            local.append(os.path.join(self.CONFIGPATH, qrc_fn))
            local.append( os.path.join(self.CONFIGPATH, 'qtvcp/screens',self.BASEPATH, qrc_fn))
            local.append(os.path.join(self.CONFIGPATH, self.BASEPATH, qrc_fn))
        else:
            local.append( os.path.join(self.WORKINGDIR, 'qtvcp/panels',self.BASEPATH, qrc_fn))
            local.append( os.path.join(self.WORKINGDIR, qrc_fn))
            defaultqrc = os.path.join(self.PANELDIR, self.BASEPATH, qrc_fn)

        for localqrc in local:
            LOG.debug("Checking for .qrc in: yellow<{}>".format(localqrc))
            if os.path.exists(localqrc):
                LOG.info("Using LOCAL qrc file from: yellow<{}>".format(localqrc))
                self.QRC = localqrc
                self.QRC_IS_LOCAL = True
                break
        # if no break
        else:
            LOG.debug("Checking for .qrc in: yellow<{}>".format(defaultqrc))
            if os.path.exists(defaultqrc):
                LOG.debug("Using DEFAULT qrc file from: yellow<{}>".format(defaultqrc))
                self.QRC = defaultqrc
                self.QRC_IS_LOCAL = False
            else:
                self.QRC = None
                LOG.info("No qrc file found.")

        # check for qrcpy file
        qrcpy_fn = 'resources.py'
        self.QRCPY_IS_LOCAL = None
        if self.IS_SCREEN:
            localqrcpy = os.path.join(self.CONFIGPATH, 'qtvcp/screens', self.BASEPATH,qrcpy_fn)
        else:
            localqrcpy = os.path.join(self.WORKINGDIR, 'qtvcp/panels', qrcpy_fn)

        LOG.debug("Checking for resources.py in: yellow<{}>".format(localqrcpy))
        # if there is a local resource file or a QRC to compile it from:
        if os.path.exists(localqrcpy) or self.QRC is not None:
            if os.path.exists(localqrcpy):
                LOG.info("Using LOCAL resources.py file from: yellow<{}>".format(localqrcpy))
            else:
                LOG.info("Resources.py file needs to be compiled at: {}".format(localqrcpy))
            self.QRCPY = localqrcpy
            self.QRCPY_IS_LOCAL = True
        else:
            self.QRCPY = None
            LOG.info("No resources.py file found, no QRC file to compile one from.")

        # translation path
        lang = QtCore.QLocale.system().name().split('_')[0]
        qm_fn = "languages/{}_{}.qm".format(self.BASEPATH,lang)
        defaultqm = os.path.join(self.SCREENDIR, self.BASEPATH, qm_fn)
        local = []
        local.append( os.path.join(self.CONFIGPATH, 'qtvcp/screens',self.BASEPATH, qrc_fn))
        local.append( os.path.join(self.CONFIGPATH, self.BASEPATH, qm_fn))

        for localqm in local:
            LOG.debug("Checking for translation file in: yellow<{}>".format(localqm))
            if os.path.exists(localqm):
                LOG.info("Using LOCAL translation file from: yellow<{}>".format(localqm))
                self.LOCALEDIR = localqm
                break
        # if no break
        else:
            LOG.debug("Checking for translation file in: yellow<{}>".format(defaultqm))
            if os.path.exists(defaultqm):
                LOG.debug("Using DEFAULT translation file from: yellow<{}>".format(defaultqm))
                self.LOCALEDIR = defaultqm
            else:
                LOG.info("Using no translations, default system locale is: yellow<{}>".format(lang))
                self.LOCALEDIR = None

        # look for ABOUT files:
        about_fn = "{}_ABOUT".format(self.BASEPATH)
        local = []
        if self.IS_SCREEN:
            default_about_path = os.path.join(self.SCREENDIR, self.BASEPATH, about_fn)
            local.append(os.path.join(self.CONFIGPATH, about_fn))
            local.append(os.path.join(self.CONFIGPATH, 'qtvcp/screens',self.BASEPATH, about_fn))
        else:
            local.append(os.path.join(self.WORKINGDIR, about_fn))
            local.append(os.path.join(self.WORKINGDIR, 'qtvcp/panels', about_fn))
            default_about_path = os.path.join(self.PANELDIR, self.BASEPATH, about_fn)

        for localabout in local:
            LOG.debug("Checking for LOCAL about file in: yellow<{}>".format(localabout))
            if os.path.exists(localabout):
                self.ABOUT = localabout
                LOG.info("Using LOCAL about file path: yellow<{}>".format(self.ABOUT))
                break
        else:
            LOG.debug("Checking for DEFAULT about file in: yellow<{}>".format(default_about_path))
            if os.path.exists(default_about_path):
                self.ABOUT = default_about_path
                LOG.debug("Using DEFAULT about file path: yellow<{}>".format(self.ABOUT))
            else:
                self.ABOUT = ""
                LOG.debug("No about file found.")

    # search for local ui paths or default to standard
    def find_widget_path(self,uifile=''):
        try:
            local = os.path.join(self.CONFIGPATH, 'qtvcp/widgets_ui', uifile)
            LOG.verbose("Checking for widget file in: yellow<{}>".format(local))
            if os.path.exists(local):
                LOG.info("Using LOCAL widget file from: yellow<{}>".format(local))
                return local
        except:
            pass

        default = os.path.join(self.WIDGETUI,uifile)
        LOG.verbose("Checking for widget file in: yellow<{}>".format(default))
        if os.path.exists(default):
            LOG.verbose("Using default widget file from: yellow<{}>".format(default))
            return default

    # search for local image paths or default to standard
    def find_image_path(self,imagefile=''):
        try:
            local = os.path.join(self.CONFIGPATH, 'qtvcp/images', imagefile)
            LOG.verbose("Checking for images path in: yellow<{}>".format(local))
            if os.path.exists(local):
                LOG.info("Using LOCAL images path from: yellow<{}>".format(local))
                return local
        except:
            pass

        default = os.path.join(self.IMAGEDIR,imagefile)
        LOG.verbose("Checking for image path in: yellow<{}>".format(default))
        if os.path.exists(default):
            LOG.verbose("Using default image path from: yellow<{}>".format(default))
            return default

    def find_embed_panel_path(self, name):
        # look for custom ui file
        ui_fn = "{}.ui".format(name)
        local = []
        local.append( os.path.join(self.WORKINGDIR, 'qtvcp/panels',name, ui_fn))
        local.append( os.path.join(self.WORKINGDIR, ui_fn))
        defaultui = os.path.join(self.PANELDIR, name, ui_fn)

        for localui in local:
            LOG.debug("(embed) Checking for .ui in: yellow<{}>".format(localui))
            if os.path.exists(localui):
                LOG.info("(embed) Using LOCAL ui file from: yellow<{}>".format(localui))
                XML = localui
                return XML
        # if no break
        else:
            LOG.debug("(embed) Checking for .ui in: yellow<{}>".format(defaultui))
            if os.path.exists(defaultui):
                LOG.info("(embed) Using DEFAULT ui file from: yellow<{}>".format(defaultui))
                XML = defaultui
                return XML
            else:
                # error
                LOG.critical("(embed) No UI file found - Did you add the .ui name/path?")
                return True # error

    def find_embed_handler_path(self, name):
        # look for custom handler files:
        handler_fn = "{}_handler.py".format(name)
        local = []
        # in standard folder
        local.append( os.path.join(self.WORKINGDIR, 'qtvcp/panels',name, handler_fn))
        # relative to configuration folder
        local.append( os.path.join(self.WORKINGDIR, handler_fn))
        # builtin panel folder
        default_handler_path = os.path.join(self.PANELDIR, name, handler_fn)

        for local_handler_path in local:
            LOG.debug("(embed) Checking for handler file in: yellow<{}>".format(local_handler_path))
            if os.path.exists(local_handler_path):
                HANDLER = local_handler_path
                LOG.info("(embed) Using LOCAL handler file path: yellow<{}>".format(HANDLER))
                return HANDLER
        # if no break
        else:
            LOG.debug("(embed) Checking for default handler file in: yellow<{}>".format(default_handler_path))
            if os.path.exists(default_handler_path):
                HANDLER = default_handler_path
                LOG.info("(embed) Using DEFAULT handler file path: yellow<{}>".format(HANDLER))
                return HANDLER
            else:
                HANDLER = None
                LOG.info("(embed) No handler file found.")
                return True

    def find_screen_dirs(self):
        dirs = next(os.walk(self.SCREENDIR))[1]
        return dirs

    def find_panel_dirs(self):
        dirs = next(os.walk(self.PANELDIR))[1]
        return dirs

    def find_vismach_files(self):
        tmp = []
        for file in os.listdir(self.VISMACHDIR):
            if file.endswith(".py"):
                if not file in ('__init__.py', 'qt_vismach.py', 'primitives.py'):
                    tmp.append(file)
            elif os.path.isdir(os.path.join(self.VISMACHDIR, file)):
                if 'obj' in file:
                    tmp.append(file)

        return tmp

    def modnamefromFilename(self, fname):
        panel = os.path.splitext(os.path.basename(os.path.basename(fname)))[0]
        base = panel.replace('_handler','')
        module = "{}.{}".format(base,panel)
        return module

    # tempararily adds the screen directory to path
    # so the handler can be imported to be used for subclassing
    def importDefaultHandler(self, module=None):
        import importlib
        sys.path.insert(0, self.SCREENDIR)
        if module is None:
            module = "{}.{}_handler".format(self.BASEPATH,self.BASEPATH)
        mod = importlib.import_module(module, self.SCREENDIR)
        sys.path.remove(self.SCREENDIR)
        return mod.HandlerClass
