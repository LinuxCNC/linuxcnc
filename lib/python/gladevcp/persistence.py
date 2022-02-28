#!/usr/bin/env python3
# vim: sts=4 sw=4 et
#    This is a component of EMC
#    util.py Copyright 2010 Michael Haberler
#
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
#    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.'''
'''
    persistence support for gladevcp widgets
    Michael Haberler 11/2010
'''
import os
import sys
import time

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from configobj import ConfigObj, flatten_errors
from validate import Validator
from hashlib import sha1
from .hal_widgets import _HalWidgetBase
from .hal_actions import _EMC_ActionBase
from gladevcp.gladebuilder import widget_name

class UselessIniError(Exception):
    pass

class BadDescriptorDictError(Exception):
    pass

# map python type names to ConfigObj type names
co_map = {'str': 'string','int':'integer', 'str': 'string', 'bool': 'boolean'}
debug = 0
version_number = 1


def warn(*args):
    print(''.join(args), file=sys.stderr)


def dbg(level,*args):
    global debug
    if debug < level: return
    print(''.join(args))


def set_debug(value):
    global debug
    debug = value


def select_widgets(widgets, hal_only=False,output_only = False):
    '''
    input: a list of widget instances
    output: a list of widget instances, pruned according to the flags:
        hal_only = True:  only add HAL widgets which make sense restoring
        output_only = True: only add widgets which let users
        set a value (scale,button..)
    '''
    wlist = []
    for w in widgets:
        # always skip the following types: 
        if isinstance(w, (_EMC_ActionBase)):
            continue
        if hal_only and not isinstance(w, _HalWidgetBase):
            continue
        if output_only and not isinstance(w, (Gtk.Range,
                                              Gtk.SpinButton,
                                              Gtk.ComboBox,
                                              Gtk.CheckButton,
                                              Gtk.ToggleButton,
                                              Gtk.RadioButton)):
            continue
        wlist.append(w)
    return wlist


def accessors(w):
    '''
    retrieve getter/setter name of an 'interesting' widget
    '''
    if isinstance(w, (Gtk.Range, Gtk.SpinButton)):
        return (w.get_value,w.set_value)
    if isinstance(w, (Gtk.CheckButton, Gtk.ToggleButton,Gtk.RadioButton,Gtk.ComboBox)):
        return (w.get_active,w.set_active)
    return (None,None)

def store_value(widget,value):
    _,setter = accessors(widget)
    return setter and setter(value)

def get_value(widget):
    getter,_ = accessors(widget)
    return getter and getter()

def widget_defaults(widgets):
    '''
    return a dict name/current value of wvalues which are to be restored
    '''
    wvalues = dict()
    for w in widgets:
        k = widget_name(w)
        try:
            v = get_value(w)
            wvalues[k] = v
        except Exception as msg:
            warn("widget_defaults:" + msg)
            continue
    return wvalues


class IniFile(object):

    # well known section and variable names in .ini files
    vars = 'vars'
    widgets = 'widgets'
    ini = 'ini'
    signature = 'signature'
    version = 'version'

    def _gen_spec(self,vdict):
        '''
        given a nested dict of sections,vars and default values,
        derive a ConfigObj typed spec
        NB: only single level nesting supported

        FIXME: add safeguards against broken vdict()
        '''
        spec = ''
        for section in sorted(vdict.keys()):
            spec += '[' + section + ']\n'
            for varname in sorted(vdict[section].keys()):
                typename = type(vdict[section][varname]).__name__
                if typename in co_map:
                    typename = co_map[typename]
                spec += '\t' + varname + ' = ' + typename  + '\n'
        return spec

    def restore_state(self,obj):
        '''
        restore attributes from ini file 'vars' section as obj attributes,
        as well as any widget state in 'widgets' section
        '''
        dbg(1, "restore_state() from %s" % (self.filename))

        if not IniFile.ini in self.defaults:
            raise BadDescriptorDictError("defaults dict lacks 'ini' section")

        if  self.defaults[IniFile.ini][IniFile.signature] != (
                self.config[IniFile.ini][IniFile.signature]):
            warn("signature mismatch in %s -  resetting to default" %
                 (self.filename))
            dbg(1, "expected: %s, got %s" %
                (self.defaults[IniFile.ini][IniFile.signature],
                 self.config[IniFile.ini][IniFile.signature]))
            self.create_default_ini()
        else:
            dbg(1,"signature verified OK for %s " % (self.filename))

        if IniFile.vars in self.config:
            for k,v in list(self.defaults[IniFile.vars].items()):
                setattr(obj,k,self.config[IniFile.vars][k])

        if IniFile.widgets in self.config:
            for k,v in list(self.config[IniFile.widgets].items()):
                store_value(self.builder.get_object(k),v)

    def save_state(self, obj):
        '''
        save obj attributes as listed in ini file 'IniFile.vars' section and
        widget state to 'widgets' section
        '''
        if IniFile.vars in self.defaults:
            for k,v in list(self.defaults[IniFile.vars].items()):
                self.config[IniFile.vars][k] = getattr(obj,k,None)

        if IniFile.widgets in self.config:
            for k in list(self.defaults[IniFile.widgets].keys()):
                self.config[IniFile.widgets][k] = get_value(self.builder.get_object(k))

        self.config.final_comment = ['last update  by %s.save_state() on %s ' %
                                         (__name__,time.asctime())]
        self.write()
        dbg(1, "save_state() to %s" % (self.filename))


    def create_default_ini(self):
        '''
        create a default ini file with defaults derived from configspec
        '''
        self.config = ConfigObj(self.filename, interpolation=False,
                                configspec=self.spec)
        vdt = Validator()
        self.config.validate(vdt, copy=True)
        self.config.update(self.defaults)
        self.config.filename = self.filename
        self.config.initial_comment.append('generated by %s.create_default_ini() on %s' %
                                           (__name__, time.asctime()))
        self.config.write()

    def read_ini(self):
        '''
        make sure current file validates OK, this will also type-convert values
        recreate default ini file if bad things happen
        '''
        retries = 2
        while True:
            try:
                self.config = ConfigObj(self.filename,
                                        file_error=True,
                                        interpolation=False,
                                        configspec=self.spec,
                                        raise_errors=True)
                validator = Validator()
                results = self.config.validate(validator,preserve_errors=True)
                if results != True:
                    for (section_list, key, error
                         ) in flatten_errors(self.config, results):
                        if key is not None:
                            warn("key '%s' in section '%s' : %s" %
                                 (key, ', '.join(section_list),
                                  error if error else "missing"))
                        else:
                            warn("section '%s' missing" %
                                 ', '.join(section_list))
                    badfilename = self.filename + '.BAD'
                    os.rename(self.filename,badfilename)
                    retries -= 1
                    if retries:
                        raise UselessIniError("can not make sense of '%s', renamed to '%s'"
                                              % (self.filename, badfilename))
                    else:
                        raise Exception(error)

            except (IOError, TypeError,UselessIniError) as msg:
                warn("%s - creating default" % (msg))
                self.create_default_ini()
                continue

            except: # pass on ConfigObj's other troubles
                raise

            else: # all great
                break

    def write(self,filename=None):
        '''
        write out changed config
        '''
        if filename:
            self.filename = filename
        self.config.write()

    def __init__(self,filename,defaults,builder):
        defaults[IniFile.ini] =   { IniFile.signature : 'astring',
                                    IniFile.version : version_number }
        self.defaults = defaults
        self.filename = filename
        self.builder = builder
        spec = self._gen_spec(self.defaults)
        self.signature = sha1(spec.encode('utf-8')).hexdigest()
        self.defaults[IniFile.ini][IniFile.signature] = self.signature

        dbg(2, "auto-generated spec:\n%s\nsignature = %s" %
            (spec, self.signature))
        self.spec = spec.splitlines()
        self.read_ini()
