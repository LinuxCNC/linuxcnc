import re

class EmcToolTable(object):

    ttype = { 'T' : int, 'P': int, 'Q':int,
              'X' : float, 'Y' : float, 'Z' : float,
              'A' : float, 'B' : float, 'C' : float,
              'U' : float, 'V' : float, 'W' : float,
              'I' : float, 'J' : float, 'D' : float }

    def __init__(self,filename,random_toolchanger):
         self.filename = filename
         self.random_toolchanger = random_toolchanger

    def load(self, tooltable,comments):
        self.fakepocket = 0
        fp = open(self.filename)
        lno = 0
        for line in fp.readlines():
            lno += 1
            if not line.startswith(';'):
                if line.strip():
                    entry = self.parseline(lno,line.strip())
                    if entry:
                        self.assign(tooltable,entry,comments)
        fp.close()

    def save(self, tooltable, comments):
        print "Save tooltable NIY"

    def assign(self,tooltable,entry,comments):
        if self.random_toolchanger:
            pocket = entry['P']
        else:
            self.fakepocket += 1
            pocket = self.fakepocket

        tooltable[pocket].zero()
        for (key,value) in entry.items():
            if key == 'T' : tooltable[pocket].toolno = value
            if key == 'Q' : tooltable[pocket].orientation = value
            if key == 'D' : tooltable[pocket].diameter = value
            if key == 'I' : tooltable[pocket].frontangle = value
            if key == 'J' : tooltable[pocket].backangle = value
            if key == 'X' : tooltable[pocket].offset.x = value
            if key == 'Y' : tooltable[pocket].offset.y = value
            if key == 'Z' : tooltable[pocket].offset.z = value
            if key == 'A' : tooltable[pocket].offset.a = value
            if key == 'B' : tooltable[pocket].offset.b = value
            if key == 'C' : tooltable[pocket].offset.c = value
            if key == 'U' : tooltable[pocket].offset.u = value
            if key == 'V' : tooltable[pocket].offset.v = value
            if key == 'W' : tooltable[pocket].offset.w = value
            if key == 'comment' : comments[pocket] = value # aaargh



    def parseline(self,lineno,line):
        """
        read a tooltable line
        if an entry was parsed successfully, return a  Tool() instance
        """
        if re.match('\A\s*T\d+',line): # an MG line
            semi = line.find(";")
            comment = line[semi+1:]
            entry = line.split(';')[0]
            gd = dict()
            for field in entry.split():
                (name,value)  = re.search('([a-zA-Z])([+-]?\d*\.?\d*)',field).groups()
                if name:
                    key = name.upper()
                    gd[key] = EmcToolTable.ttype[key](value)
                else:
                    print "%s:%d  bad line: '%s' " % (self.filename, lineno, entry)
            gd['comment'] = comment
            return gd
        print "%s:%d: unrecognized tool table entry   '%s'" % (self.filename,lineno,line)
