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

cp = ConfigParser.ConfigParser
class preferences(cp):
    types = {
        bool: cp.getboolean,
        float: cp.getfloat,
        int: cp.getint,
        str: cp.get,
        repr: lambda self,section,option: eval(cp.get(self,section,option)),
    }

    def __init__(self):
        cp.__init__(self)
        self.fn = os.path.expanduser("~/.touchy_preferences")
        self.read(self.fn)

    def getpref(self, option, default=False, type=bool):
        m = self.types.get(type)
        try:
            o = m(self, "DEFAULT", option)
        except Exception, detail:
            print detail
            self.set("DEFAULT", option, default)
            self.write(open(self.fn, "w"))
            o = default
        return o

    def putpref(self, option, value, type=bool):
        self.set("DEFAULT", option, type(value))
        self.write(open(self.fn, "w"))
