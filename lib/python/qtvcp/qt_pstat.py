#!/usr/bin/env python
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

# Set up logging
import logger
LOG = logger.getLogger(__name__)
# Set the log level for this module
LOG.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

# BASE is the absolute path to linuxcnc base
# LIBDIR is the path to qtvcp python files
# DATADIR is where the standarad UI files are
# IMAGEDIR is for icons
class _PStat(object):
    def __init__(self):
        # only initialize once for all instances
        if self.__class__._instanceNum >=1:
            return
        self.__class__._instanceNum += 1

    def set_paths(self, filename='dummy', isscreen = False):
        self.PREFS_FILENAME = None
        self.WORKINGDIR = os.getcwd()
        self.IS_SCREEN = isscreen
        if isscreen:
            # path to the configuration the user requested
            self.CONFIGPATH = os.environ['CONFIG_DIR']
            sys.path.insert(0, self.CONFIGPATH)
        else:
            # VCP panels don't usually have config paths but QTVCP looks for one.
            # TODO this fixes the error but maybe it should be something else
            self.CONFIGPATH = self.WORKINGDIR

        # Linuxcnc project base directory
        self.BASEDIR = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
        # PyQt's .ui file's basename 
        self.BASENAME = os.path.splitext(os.path.basename(filename))[0]
        LOG.debug('BASENAME {}'.format(self.BASENAME))
        # python library directory
        self.LIBDIR = os.path.join(self.BASEDIR, "lib", "python")
        sys.path.insert(0, self.LIBDIR)
        self.IMAGEDIR = os.path.join(self.BASEDIR, "share","qtvcp","images")
        self.SCREENDIR = os.path.join(self.BASEDIR, "share","qtvcp","screens")
        self.PANELDIR = os.path.join(self.BASEDIR, "share","qtvcp","panels")

        # look for custom handler files:
        handler_fn = "{}_handler.py".format(self.BASENAME)
        if self.IS_SCREEN:
            default_handler_path = os.path.join(self.SCREENDIR, self.BASENAME, handler_fn)
            local_handler_path = 'None Found'
            for (root,dirs,files) in os.walk(self.CONFIGPATH, topdown=True):
                if handler_fn in(files):
                    local_handler_path = os.path.join(root, handler_fn)
                    break
        else:
            local_handler_path = os.path.join(self.WORKINGDIR, handler_fn)
            default_handler_path = os.path.join(self.PANELDIR, self.BASENAME, handler_fn)
        LOG.debug("Checking for handler file in: yellow<{}>".format(local_handler_path))

        if os.path.exists(local_handler_path):
            self.HANDLER = local_handler_path
            LOG.info("Using specified handler file path: yellow<{}>".format(self.HANDLER))
        else:
            LOG.debug("Checking for default handler file in: yellow<{}>".format(default_handler_path))
            if os.path.exists(default_handler_path):
                self.HANDLER = default_handler_path
                LOG.info("Using default handler file path: yellow<{}>".format(self.HANDLER))
            else:
                self.HANDLER = None
                LOG.info("No handler file found")

        # look for custom ui file
        ui_fn = "{}.ui".format(self.BASENAME)
        if self.IS_SCREEN:
            defaultui = os.path.join(self.SCREENDIR, self.BASENAME, ui_fn)
            localui = 'None Found'
            for (root,dirs,files) in os.walk(self.CONFIGPATH, topdown=True):
                if ui_fn in(files):
                    localui = os.path.join(root, ui_fn)
                    break
        else:
            localui = os.path.join(self.WORKINGDIR, ui_fn)
            defaultui = os.path.join(self.PANELDIR, self.BASENAME, ui_fn)
        LOG.debug("Checking for .ui in: yellow<{}>".format(localui))
        if os.path.exists(localui):
            LOG.info("Using specified ui file from yellow<{}>".format(localui))
            self.XML = localui
        else:
            LOG.debug("Checking for .ui in: yellow<{}>".format(defaultui))
            if os.path.exists(defaultui):
                LOG.info("Using DEFAULT ui file from yellow<{}>".format(defaultui))
                self.XML = defaultui
            else:
                # error
                self.XML = None
                LOG.critical("No UI file found - Did you add the .ui name/path?")
                print('\n')
                if self.IS_SCREEN:
                    dirs = next(os.walk(self.SCREENDIR))[1]
                    LOG.error('Available built-in Machine Control Screens:')
                    for i in dirs:
                        print('{}'.format(i))
                else:
                    dirs = next(os.walk(self.PANELDIR))[1]
                    LOG.error('Available built-in VCP Panels:')
                    for i in dirs:
                        print('{}'.format(i))
                print('\n')
                sys.exit(0)

        # check for qss file
        qss_fn = "{}.qss".format(self.BASENAME)
        if self.IS_SCREEN:
            defaultqss = os.path.join(self.SCREENDIR, self.BASENAME, qss_fn)
            localqss = 'None Found'
            for (root,dirs,files) in os.walk(self.CONFIGPATH, topdown=True):
                if qss_fn in(files):
                    localqss = os.path.join(root, qss_fn)
                    break
        else:
            localqss = os.path.join(self.WORKINGDIR, qss_fn)
            defaultqss = os.path.join(self.PANELDIR, self.BASENAME, qss_fn)

        LOG.debug("Checking for .qss in: yellow<{}>".format(localqss))
        if os.path.exists(localqss):
            LOG.info("Using specified qss file from yellow<{}>".format(localqss))
            self.QSS = localqss
        else:
            LOG.debug("Checking for .qss in: yellow<{}>".format(defaultqss))
            if os.path.exists(defaultqss):
                LOG.info("Using DEFAULT qss file from yellow<{}>".format(defaultqss))
                self.QSS = defaultqss
            else:
                self.QSS = None
                LOG.info("No qss file found")

    def add_screen_paths(self):
        # check for a local translation folder
        locallocale = os.path.join(self.CONFIGPATH,"locale")
        if os.path.exists(locallocale):
            self.LOCALEDIR = locallocale
            self.DOMAIN = self.BASENAME
            LOG.debug("CUSTOM locale name = {} {}".format(self.LOCALEDIR,self.BASENAME))
        else:
            locallocale = os.path.join(self.SCREENDIR,"%s/locale"% self.BASENAME)
            if os.path.exists(locallocale):
                self.LOCALEDIR = locallocale
                self.DOMAIN = self.BASENAME
                LOG.debug("SKIN locale name = {} {}".format(self.LOCALEDIR,self.BASENAME))
            else:
                self.LOCALEDIR = os.path.join(self.BASEDIR, "share", "locale")
                self.DOMAIN = "linuxcnc"

