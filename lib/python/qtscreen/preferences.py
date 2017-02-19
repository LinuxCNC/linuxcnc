# Touchy is Copyright (c) 2009  Chris Radek <chris@timeguy.com>
#
# Touchy is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# Touchy is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.



import os, ConfigParser

cp = ConfigParser.RawConfigParser
cp.optionxform=str
class Access(cp):
    types = {
        bool: cp.getboolean,
        float: cp.getfloat,
        int: cp.getint,
        str: cp.get,
        repr: lambda self,section,option: eval(cp.get(self,section,option)),
    }

    def __init__(self,path=None):
        cp.__init__(self)
        if not path:
            path="~/.qtscreen_preferences"
        self.fn = os.path.expanduser(path)
        self.read(self.fn)

    def getpref(self, option, default=False, type=bool, section="DEFAULT"):
        m = self.types.get(type)
        try:
            o = m(self, section, option)
        except Exception, detail:
            print detail
            try:
                self.set(section, option, default)
            except ConfigParser.NoSectionError:
                print 'Adding section %s'%section
                # Create non-existent section
                self.add_section(section)
                self.set(section, option, default)
            self.write(open(self.fn, "w"))
            if type in(bool,float,int):
                o = type(default)
            else:
                o = default
        return o

    def putpref(self, option, value, type=bool, section="DEFAULT"):
        try:
            self.set(section, option, type(value))
        except ConfigParser.NoSectionError:
            print 'Adding section %s'%section
            # Create non-existent section
            self.add_section(section)
            self.set(section, option, type(value))
        self.write(open(self.fn, "w"))
