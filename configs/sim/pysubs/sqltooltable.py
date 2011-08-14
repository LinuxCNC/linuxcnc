import os
import re
import pyodbc
import emc # for ini file access only
import sys, traceback

class SqlToolTable(object):
    '''
    beginnings of ODBC tooltable access
    '''

    def __init__(self,inifile,random_toolchanger):
        print "SQL tt init"
        self.inifile = inifile
        self.random_toolchanger = random_toolchanger
        self.connectstring = self.inifile.find("TOOL", "ODBC_CONNECT")
        self.conn = pyodbc.connect(self.connectstring)
        self.cursor = self.conn.cursor()

    def load(self, tooltable,comments,fms):
        try:
            self.cursor.execute("select * from tools;")

            #cols = [t[0] for t in self.cursor.description]

            for row in self.cursor.fetchall():
                pocket = row.pocket
                t = tooltable[pocket]
                t.toolno = row.toolno
                t.orientation = row.orientation
                t.diameter = row.diameter
                t.frontangle = row.frontangle
                t.backangle = row.backangle
                t.offset.tran.x = row.x_offset
                t.offset.y = row.y_offset
                t.offset.z = row.z_offset
                t.offset.a = row.a_offset
                t.offset.b = row.b_offset
                t.offset.c = row.c_offset
                t.offset.u = row.u_offset
                t.offset.v = row.v_offset
                t.offset.w = row.w_offset

        except pyodbc.Error, e:
            traceback.print_exc(file=sys.stdout)
        else:
            start = 0 if self.random_toolchanger else 1
            for p in range(start,len(tooltable)):
                t = tooltable[p]
                if t.toolno != -1: print str(t)

    def save(self, tooltable, comments,fms):
        print "SQL save",
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
            print "done"

        except pyodbc.Error, msg:
            traceback.print_exc(file=sys.stdout)
            print "saving tooltable to %s failed" % (self.filename)


    def save_state(self,e):
        print "SQL save_state",
        try:

            self.cursor.execute("delete from state;")
            self.cursor.execute("insert into state values(?,?);",e.io.tool.toolInSpindle,e.io.tool.pocketPrepped)
            self.cursor.execute("commit;")
            print "done"

        except pyodbc.Error, msg:
            print "save_state() failed:"
            traceback.print_exc(file=sys.stdout)
        pass

    def restore_state(self,e):
        print "SQL save_state",
        try:
            self.cursor.execute("select tool_in_spindle,pocket_prepped from state;")
            row = self.cursor.fetchone()
            e.io.tool.pocketPrepped = row.pocket_prepped
            e.io.tool.toolInSpindle = row.tool_in_spindle
            print "restored tool=%d pocket=%d" % (row.tool_in_spindle,row.pocket_prepped)

        except pyodbc.Error, msg:
            print "restore_state() failed:"
            traceback.print_exc(file=sys.stdout)
        pass
        pass
