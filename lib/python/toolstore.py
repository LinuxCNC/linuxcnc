#!/usr/bin/python2
# -*- coding: utf-8 -*-

import linuxcnc
import sqlite3 as sql
import sys
import re
import os

##  FIXME  #
sql_path = os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sql_path = os.path.join(sql_path, "share/sql")
print sql_path

def dict_into_query(q, d):
    "replaces each occurence of :key in the input file with the textual representation of the value of d[key]"

    f = re.search(':([\w]+)', q)
    while f:
        if f.group(1) in d:
            if d[f.group(1)]:
                q = q[:f.start()] + str(d[f.group(1)]) + q[f.end():]
            else:
                q = q[:f.start()] + "NULL" + q[f.end():]
        else: #Tags that are in the query, but not the dict
            q = q[:f.start()] + "NULL" + q[f.end():] 
        f = re.search(':([\w]+)', q)
         
    return q

class toolstore:
    """An attempt to provide configurable access to user-configurable tool 
    data"""
    
    def __get_script(self, sub_script_name):
        "Utility function to extract queries separated by pseudo-XML tags"
        q = '<{0}>(.+?)</{0}>'.format(sub_script_name)
        regex = re.compile(q, flags=re.MULTILINE | re.DOTALL)
        script = regex.search(self.scripts)
        if script != None:
            return script.group(1)
        else:
            print "sub-script {0} not found".format(sub_script_name)
    
    def __init__(self):
        ini_name = os.getenv('INI_FILE_NAME')
        self.inifile=linuxcnc.ini(ini_name)
        base_path = os.getenv('CONFIG_DIR')
        sql_path = os.path.join(os.getenv('LINUXCNC_HOME'), 'share','sql')
        
        print "INI file = " + ini_name
        print "base_path = ", base_path
        print "sql path = ", sql_path

        script_name = self.inifile.find("TOOL", "SQL")
        if script_name == None:
            print """No SQL file specified in the [TOOL] section of the INI
            file. Using the default file"""
            script_name = "default"
        script_name = os.path.splitext(script_name)[0]
        script_name = os.path.join(sql_path, script_name + '.sql')
        if os.path.exists(script_name):
            self.scripts = open(script_name).read()
        else:
            print "Can't find the file {0}".format(script_name)     
            exit(0)
           
        db_name = self.inifile.find("TOOL", "TOOL_DB")
        
        if db_name:
            db_name = os.path.join(base_path, db_name)
        else:
            #no database name given
            print "No tool database specified in ini, trying tool_db"
            db_name = os.path.join(base_path, "tool_db")
        print "Database name == " + db_name
        
        self.db = None
        try: 
            self.db = sql.connect(db_name)
            print "opened (or created) tool database " + os.getenv('CONFIGDIR', '') + db_name
        except:
            #Database does not exist
            print "Error: Can't open or create the database"
            
        self.db.row_factory = sql.Row    
        self.cur = self.db.cursor()
        self.cur.execute('SELECT count(*) FROM sqlite_master')
        check = self.cur.fetchone()

        if not check[0]:
            #db exists, but is empty
            print "Database " + db_name + " is empty"
            schema = self.__get_script('schema')
            print "schema==", schema
            self.cur.executescript(schema)
            
            tbl_name = self.inifile.find("EMCIO","TOOL_TABLE")
            if tbl_name:
                print "found tool table " + tbl_name + " in INI"
            else:
                print "No tool database and no tool table name, using tool.tbl"
                tbl_name = "tool.tbl"
            if not os.path.exists(tbl_name):
                print "Failed to open tool table, deleting tool database "
                os.remove(db_name)
            
            self.import_tbl(tbl_name)
            
    def import_tbl(self, tbl_name):
        "Imports an old-style tool table into the database"
        import_script = self.__get_script('import')
        tbl = open(tbl_name)
        for t in tbl.readlines():
            tool = {'Tool':re.search('[Tt]([0-9+-.]+) ', t),
                    'Pocket':re.search('[Pp]([0-9+-.]+)', t),
                    'X':re.search('[Xx]([0-9+-.]+)', t),
                    'Y':re.search('[Yy]([0-9+-.]+)', t),
                    'Z':re.search('[Zz]([0-9+-.]+)', t),
                    'A':re.search('[Aa]([0-9+-.]+)', t),
                    'B':re.search('[Bb]([0-9+-.]+)', t),
                    'C':re.search('[Cc]([0-9+-.]+)', t),
                    'U':re.search('[Uu]([0-9+-.]+)', t),
                    'V':re.search('[Vv]([0-9+-.]+)', t),
                    'W':re.search('[Ww]([0-9+-.]+)', t),
                    'Diameter':re.search('[Dd]([0-9+-.]+)', t),
                    'Frontangle':re.search('[Ii]([0-9+-.]+)', t),
                    'Backangle':re.search('[Jj]([0-9+-.]+)', t),
                    'Orientation':re.search('[Qq]([0-9+-.]+)', t),
                    'Comment':re.search(';(.*)', t)}
            for m in tool:
                if tool[m]:
                    tool[m] = tool[m].group(1)
                        
            temp_script = dict_into_query(import_script, tool)
            self.cur.executescript(temp_script)
        
    def get_offset(self, h):
        """Takes a single integer (as might be provided by G43 Hnn) and uses it to apply an offset
        returns a tuple of offsets"""
        self.cur.execute(self.__get_script("get_offset"), {'H':h})
        r = self.cur.fetchone()
        if r == None:
            offs = {'Error':'Offset %d can not be found in the tool database' % h}
            return offs
        ret = {}
        for k in r.keys():
            if r[k] == None:
                ret[k] = 0
            else:
                ret[k] = r[k]
        return ret
    
    def find_tool(self, t):
        """Uses the T-number to find a toolID"""
        self.cur.execute(self.__get_script("find_tool"), {'T':t})
        r = self.cur.fetchone()
        if r == None:
            ret = {'Error':'Tool %d can not be found in the tool database' % t}
            return ret
        ret = {}
        for k in r.keys():
            if r[k] == None:
                ret[k] = -1
            else:
                ret[k] = r[k]
        return ret


        
        
        
                
            
        
        
        
        
    
