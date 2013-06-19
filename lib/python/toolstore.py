#!/usr/bin/python2
# -*- coding: utf-8 -*-

import linuxcnc
import sqlite3 as sql
import sys
import re
import os

sql_path = os.path.dirname(os.path.dirname(os.path.dirname(os.path.realpath(__file__))))
sql_path = os.path.join(sql_path, "share/sql")
print sql_path
def add_ext(f, ext):
    f = os.path.splitext(f)[0]
    return os.path.join(sql_path, f + ext)

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

def get_script(self, ini_tag, default_file):
    script_name = self.inifile.find("TOOL", ini_tag)
    if not script_name:
        script_name = default_file
    script_name = add_ext(script_name, ".sql")
    try:  
        script = open(script_name).read()
    except:
        print "The file " + script_name + " requested in the INI file does not exist"
        print "using " + default_file + " instead"
        try: 
            script = open(default_file).read()
        except:
            print "Can't find the default script either, bailing out"
            exit(0)
    return script
      
class toolstore:
    """An attempt to provide configurable access to user-configurable tool 
    data"""
    def __init__(self):
        self.inifile=linuxcnc.ini('~/classic.ini')
        dbname = self.inifile.find("TOOL", "TOOL_DB")
        print "opening tool database " + dbname
        db = None
        
        if not dbname:
            #no database name given
            print "No tool database specified in ini, trying tool_db"
            dbname = "tool_db"
        try: 
            self.db = sql.connect(dbname)
            print "opened tool database " + dbname
        except:
            #Database does not exist
            print "Error: Can't open or create the database"
            
        self.db.row_factory = sql.Row    
        self.cur = self.db.cursor()
        self.cur.execute('SELECT count(*) FROM sqlite_master WHERE tbl_name = "tools" AND type = "table"')
        check = self.cur.fetchone()
        print check
        if not check[0]:
            #db exists, but is empty
            print "Database " + dbname + " exists, but is empty"
            schema = get_script(self, "DEFAULT_SCHEMA", "classic_schema.sql")  
            self.cur.executescript(schema)

            import_script = get_script(self, "IMPORT_SCRIPT", "classic_import.sql")
              
            tblname = self.inifile.find("EMCIO","TOOL_TABLE")
            if tblname:
                print "found tool table " + tblname + " in INI"
            else:
                print "No tool database and no tool table name, using tool.tbl"
                tblname = "tool.tbl"
            try:
                tbl = open(tblname)
            except:
                print "Failed to open tool table, deleting tool database "
                os.remove(dbname)
                
            for t in tbl.readlines():
                tool = {'Tool':re.search('[Tt]([0-9+-.]+) ',t),
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
                
        #set up the other utility scripts according to the INI options. 
        self.g43_script = get_script(self, "G43_SCRIPT", "classic_g43")
        
    def choose_offset_by_num(self, h):
        """Takes a single integer (as might be provided by G43 Hnn) and uses it to apply an offset
        returns a tuple of offsets"""
        temp_script = dict_into_query(self.g43_script, {'H': h})
        print "Working G43 script " + temp_script
        self.cur.execute(temp_script)
        r = self.cur.fetchone()
        print self.cur.description
        return r
    
    def find_tool(self, t):
        """Uses the T-number to find a toolID"""
        temp_script = get_script(self, "TOOL_SCRIPT", "classic_select_tool")
        temp_script = dict_into_query(temp_script, {'T': t})
        print "Working toolchange script  " + temp_script
        self.cur.execute(temp_script)
        r = self.cur.fetchone()
        print r.keys()
        print r
        return r


        
        
        
                
            
        
        
        
        
    
