#!/usr/bin/env python

# QTVcp Logging Module
# Provides a consistent and easy to use logging facility.  Log messages printed
# to the terminal will be colorized for easy identification of log level.
#
# Copyright (c) 2017 Kurt Jacobson
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


import os
import logging
from linuxcnc import ini

# For convenience import log levels so we don't need to import
# logging to set the log level within other modules.
from logging import DEBUG, INFO, WARNING, ERROR, CRITICAL

# Our custom colorizing formatter for the terminal handler
from lib.colored_formatter import ColoredFormatter


# Global name of the base logger
BASE_LOGGER_NAME = None

# Define the log message formats
TERM_FORMAT = '[%(name)s][%(levelname)s]  %(message)s (%(filename)s:%(lineno)d)'
FILE_FORMAT = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'


# Get logger for module based on module.__name__
def getLogger(name):
    if BASE_LOGGER_NAME is None:
        initBaseLogger('QtDesigner')
    name = '{0}.{1}'.format(BASE_LOGGER_NAME, name.upper())
    return logging.getLogger(name)


# Set global logging level
def setGlobalLevel(level):
    base_log = logging.getLogger(BASE_LOGGER_NAME)
    base_log.setLevel(level)
    base_log.info('Base log level set to {}'.format(level))


# Initialize the base logger
def initBaseLogger(name, log_file=None, log_level=DEBUG):

    global BASE_LOGGER_NAME
    BASE_LOGGER_NAME = name

    if not log_file:
        log_file = getLogFile(name)

    # Clear the previous sessions log file
    with open(log_file, 'w') as fh:
        pass

    # Create base logger
    base_log = logging.getLogger(BASE_LOGGER_NAME)
    base_log.setLevel(log_level)

    # Add console handler
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    cf = ColoredFormatter(TERM_FORMAT)
    ch.setFormatter(cf)
    base_log.addHandler(ch)

    # Add file handler
    fh = logging.FileHandler(log_file)
    fh.setLevel(logging.DEBUG)
    ff = logging.Formatter(FILE_FORMAT)
    fh.setFormatter(ff)
    base_log.addHandler(fh)

    # Get logger for logger
    log = getLogger(__name__)
    base_log.info('Logging to "{}"'.format(log_file))

    return base_log


# Attempt to find the log file specified INI [DISPLAY] LOG_FILE,
# failing that log to $HOME/<base_log_name>.log
def getLogFile(name):

    # Default log file to use if not specified in INI
    log_file = os.path.expanduser('~/{}.log').format(name.lower())

    # LinuxCNC may not be running, so use get() to avoid a KeyError
    ini_file = os.environ.get('INI_FILE_NAME')
    config_dir = os.environ.get('CONFIG_DIR')

    if ini_file:
        lcnc_ini = ini(ini_file)
        path = lcnc_ini.find('DISPLAY', 'LOG_FILE')
        if path:
            if path.startswith('~'):
                # Path is relative to $HOME
                log_file = os.path.expanduser(path)
            elif not os.path.isabs(path):
                # Assume intended path is relative to the INI file
                log_file = os.path.join(config_dir, path)
            else:
                # It must be an absolute path then
                log_file = os.path.realpath(path)

    return log_file
