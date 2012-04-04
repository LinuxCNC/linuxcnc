import os
import re
import pyodbc
try:
    import emc
except ImportError:
    import linuxcnc as emc  # ini only
import sys, traceback

class SqlToolAccess(object):
    '''
    beginnings of ODBC tooltable access
    '''

    def __init__(self,inifile,random_toolchanger):
        print "SQL tt init"
        self.inifile = inifile
        self.random_toolchanger = random_toolchanger
        self.persist =  int(self.inifile.find("TOOL", "SAVE_TOOLSTATE") or 0)

        self.connectstring = self.inifile.find("TOOL", "ODBC_CONNECT")
        conn = pyodbc.connect(self.connectstring)
        cursor = conn.cursor()
        print "tables in database %s:" % (self.connectstring)
        for row in cursor.tables():
            print row.table_name
        cursor.close()
        conn.close()


    def load_table(self, tooltable,comments,fms):
        ''' populate the table'''
        try:
            conn = pyodbc.connect(self.connectstring)
            conn.autocommit = True

            cursor = conn.cursor()
            cursor.execute("select * from tools;")

            for row in cursor.fetchall():
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
        finally:
            cursor.close()
            conn.close()

    def save_table(self, tooltable, comments,fms):
        try:
            conn = pyodbc.connect(self.connectstring)
            conn.autocommit = True
            cursor = conn.cursor()
            cursor.execute("delete from tools;")
            start = 0 if self.random_toolchanger else 1
            for p in range(start,len(tooltable)):
                t = tooltable[p]
                if t.toolno != -1:
                    cursor.execute("insert into tools values(?,?,?,?,?,?,'?',?,?,?,?,?,?,?,?,?);",
                                        (t.toolno, p, t.diameter, t.backangle,t.frontangle,t.orientation,
                                         comment[p],t.offset.x,t.offset.y,t.offset.z,t.offset.a,t.offset.b,
                                         t.offset.c,t.offset.u,t.offset.v,t.offset.w))
        except pyodbc.Error, msg:
            print "saving tooltable failed:"
            traceback.print_exc(file=sys.stdout)
        finally:
            cursor.close()
            conn.close()

    def save_state(self,e):
        if not self.persist:
            return
        print "SQL save_state",
        try:
            conn = pyodbc.connect(self.connectstring)
            conn.autocommit = True
            cursor = conn.cursor()
            cursor.execute("delete from state;")
            cursor.execute("insert into state (tool_in_spindle,pocket_prepped) values(?,?)", e.io.tool.toolInSpindle,e.io.tool.pocketPrepped)
            print "done"

        except pyodbc.Error, msg:
            print "save_state() failed:"
            traceback.print_exc(file=sys.stdout)
        finally:
            cursor.close()
            conn.close()


    def restore_state(self,e):
        if not self.persist:
            return

        try:
            conn = pyodbc.connect(self.connectstring)
            conn.autocommit = True
            cursor = conn.cursor()
            if not cursor.tables(table='state').fetchone():
                print "trying to restore, but no 'state' table found"
                return
            row = cursor.execute("select tool_in_spindle,pocket_prepped from state;").fetchone()
            if row:
                e.io.tool.pocketPrepped = row.pocket_prepped
                e.io.tool.toolInSpindle = row.tool_in_spindle
                print "restored tool=%d pocketPrepped=%d" % (row.tool_in_spindle,row.pocket_prepped)
            else:
                print "no saved state record found"

        except pyodbc.Error, msg:
            print "restore_state() failed:"
            traceback.print_exc(file=sys.stdout)
        finally:
            cursor.close()
            conn.close()


