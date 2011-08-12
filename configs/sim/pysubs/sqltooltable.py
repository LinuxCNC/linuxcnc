import os
import re
import pyodbc

class SqlToolTable(object):
    '''

    '''

    ttype = { 'T' : int, 'P': int, 'Q':int,
              'X' : float, 'Y' : float, 'Z' : float,
              'A' : float, 'B' : float, 'C' : float,
              'U' : float, 'V' : float, 'W' : float,
              'I' : float, 'J' : float, 'D' : float }

    def __init__(self,filename,random_toolchanger):
        print "SQL tt init"
        self.filename = filename
        self.random_toolchanger = random_toolchanger
        self.conn = pyodbc.connect('Driver=SQLite3;Database=' + self.filename)
        self.cursor = self.conn.cursor()

    def load(self, tooltable,comments,fms):
        try:
            self.cursor.execute("select * from tools;")

            cols = [t[0] for t in self.cursor.description]
            for row in self.cursor.fetchall():
                pocket = row.pocket
                t = tooltable[pocket]
                if row.toolno: t.toolno = row.toolno
                if row.orientation: t.orientation = row.orientation
                if row.diameter: t.diameter = row.diameter
                if row.frontangle: t.frontangle = row.frontangle
                if row.backangle: t.backangle = row.backangle
                if row.x_offset: t.offset.tran.x = row.x_offset
                if row.y_offset: t.offset.y = row.y_offset
                if row.z_offset: t.offset.z = row.z_offset
                if row.a_offset: t.offset.a = row.a_offset
                if row.b_offset: t.offset.b = row.b_offset
                if row.c_offset: t.offset.c = row.c_offset
                if row.u_offset: t.offset.u = row.u_offset
                if row.v_offset: t.offset.v = row.v_offset
                if row.w_offset: t.offset.w = row.w_offset
        except Exception,e:
            print "---- load:",e
        else:
            start = 0 if self.random_toolchanger else 1
            for p in range(start,len(tooltable)):
                t = tooltable[p]
                if t.toolno != -1: print str(t)

    def save(self, tooltable, comments,fms):
        print "SQL save"
        try:
            self.cursor.execute("delete from tools;")
            start = 0 if self.random_toolchanger else 1
            for p in range(start,len(tooltable)):
                t = tooltable[p]
                if t.toolno != -1:
                    self.cursor.execute("insert into tools values(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);",
                                        (t.toolno, p, t.diameter, t.backangle,t.frontangle,t.orientation,
                                         'xxx',t.offset.x,t.offset.y,t.offset.z,t.offset.a,t.offset.b,
                                         t.offset.c,t.offset.u,t.offset.v,t.offset.w))

            self.cursor.execute("commit;")
        except Exception,e:
            print "---- save:",e
