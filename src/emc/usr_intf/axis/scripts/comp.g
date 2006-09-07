#!/usr/bin/python
#    This is 'comp', a tool to write HAL boilerplate
#    Copyright 2006 Jeff Epler <jepler@unpythonic.net>
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
#    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import sys

%%
parser Hal:
    ignore: "//.*"
    ignore: "/[*](.|\n)*?[*]/"
    ignore: "[ \t\r\n]+"

    token END: ";;"
    token PARAMDIRECTION: "rw|r"
    token PINDIRECTION: "in|out|io"
    token TYPE: "float|bit|u32|s32|u16|s16|u8|s8"
    token NAME: "[a-zA-Z_][a-zA-Z0-9_]*"
    token NUMBER: "[0-9]+|0x[0-9a-fA-F]+"

    rule File: Declaration* "$" {{ return True }}
    rule Declaration:
        "component" NAME ";" {{ comp(NAME); }}
      | "pin" NAME TYPE PINDIRECTION ";"  {{ pin(NAME, TYPE, PINDIRECTION) }}
      | "param" NAME TYPE PARAMDIRECTION ";" {{ param(NAME, TYPE, PARAMDIRECTION) }}
      | "function" NAME OptFP ";"       {{ function(NAME, OptFP) }}
      | "option" NAME Value ";"   {{ option(NAME, Value) }}

    rule OptFP: "fp" {{ return 1 }} | "nofp" {{ return 0 }} | {{ return 1 }}
    rule Value: "yes" {{ return 1 }} | "no" {{ return 0 }}  
                | NAME {{ return NAME }}
                | NUMBER {{ return int(NUMBER,0) }}

%%

def parse(rule, text, filename=None):
    P = Hal(HalScanner(text, filename=filename))
    return runtime.wrap_error_reporter(P, rule)

dirmap = {'r': 'HAL_RO', 'rw': 'HAL_RW', 'in': 'HAL_IN', 'out': 'HAL_OUT', 'io': 'HAL_IO' }

functions = []; params = []; pins = []; options = {}
comp_name = None

def comp(name):
    global comp_name
    comp_name = name;

def pin(name, type, dir):
    pins.append((name, type, dir))
def param(name, type, dir):
    params.append((name, type, dir))
def function(name, fp):
    functions.append((name, fp))
def option(name, value):
    options[name] = value

def prologue(f):
    print >> f, """\
#include "rtapi.h"
#include "rtapi_app.h"
#include "hal.h"

static int comp_id;

#define FUNCTION(name) static void name(struct state *arg, long period)
"""
    names = {}

    has_data = options.get("data")

    print >>f
    print >>f, "struct state {"
    for name, type, dir in pins:
        if names.has_key(name):
            raise RuntimeError, "Duplicate pin name: %s" % name
        print >>f, "    hal_%s_t *%s;" % (type, name)
        names[name] = 1

    for name, type, dir in params:
        if names.has_key(name):
            raise RuntimeError, "Duplicate param name: %s" % name
        print >>f, "    hal_%s_t %s;" % (type, name)
        names[name] = 1

    if has_data:
        print >>f, "    void *_data;"

    print >>f, "};"
    
    print >>f
    names = {}
    for name, fp in functions:
        if names.has_key(name):
            raise RuntimeError, "Duplicate function name: %s" % name
        print >>f, "static void %s(struct state *arg, long period);" % name
        names[name] = 1

    print >>f, "static int get_data_size(void);"

    print >>f
    print >>f, "static int export(char *prefix) {"
    print >>f, "    char buf[HAL_NAME_LEN + 2];"
    print >>f, "    int r = 0;"
    print >>f, "    int sz = sizeof(struct state) + get_data_size();"
    print >>f, "    struct state *arg = hal_malloc(sz);"
    for name, type, dir in pins:
        print >>f, "    r = hal_pin_%s_newf(%s, &(arg->%s), comp_id," % (
            type, dirmap[dir], name)
        print >>f, "        \"%%s.%s\", prefix);" % (name.replace("_", "-"))
        print >>f, "    if(r != 0) return r;"

    for name, type, dir in params:
        print >>f, "    r = hal_param_%s_newf(%s, &(arg->%s), comp_id," % (
            type, dirmap[dir], name)
        print >>f, "        \"%%s.%s\", prefix);" % (name.replace("_", "-"))
        print >>f, "    if(r != 0) return r;"

    for name, fp in functions:
        print >>f, "    rtapi_snprintf(buf, HAL_NAME_LEN, \"%%s.%s\", prefix);"\
            % name
        print >>f, "    r = hal_export_funct(buf, (void(*)(void *arg, long))%s, arg, %s, 0, comp_id);" % (
            name, int(fp))
        print >>f, "    if(r != 0) return r;"
    print >>f, "    return 0;"
    print >>f, "}"

    if options.get("count_function"):
        print >>f, "static int get_count(void);"

    if options.get("rtapi_app", 1):
        if not options.get("singleton") and not options.get("count_function") :
            print >>f, "static int count = %s;" \
                % options.get("default_count", 1)
            print >>f, "MODULE_PARM(count, \"i\");"
            print >>f, "MODULE_PARM_DESC(count, \"number of %s\");" % comp_name

        print >>f, "int rtapi_app_main(void) {"
        print >>f, "    int r = 0;"
        if not options.get("singleton"):
            print >>f, "    int i;"
        if options.get("count_function"):
            print >>f, "    int count = get_count();"
        print >>f, "    comp_id = hal_init(\"%s\");" % comp_name
        print >>f, "    if(comp_id < 0) return comp_id;"

        if options.get("singleton"):
            print >>f, "    r = export(\"%s\");" % \
                    comp_name.replace("_", "-")
        else:
            print >>f, "    for(i=0; i<count; i++) {"
            print >>f, "        char buf[HAL_NAME_LEN + 2];"
            print >>f, "        rtapi_snprintf(buf, HAL_NAME_LEN, " \
                                        "\"%s.%%d\", i);" % \
                                    comp_name.replace("_", "-")
            print >>f, "        r = export(buf);"
            print >>f, "        if(r != 0) break;"
            print >>f, "    }"
        print >>f, "    if(r) hal_exit(comp_id);"
        print >>f, "    return r;";
        print >>f, "}"

        print >>f
        print >>f, "void rtapi_app_exit(void) { hal_exit(comp_id); }"

    print >>f
    if not options.get("no_convenience_defines"):
        for name, type, dir in pins:
            print >>f, "#define %s (*arg->%s)" % (name, name)
        for name, type, dir in params:
            print >>f, "#define %s (arg->%s)" % (name, name)
        
    if has_data:
        print >>f, "#define data (*(%s*)&(arg->_data))" % options['data']
    print >>f
    print >>f

def epilogue(f):
    data = options.get('data')
    print >>f
    if data:
        print >>f, "static int get_data_size(void) { return sizeof(%s); }" % data
    else:
        print >>f, "static int get_data_size(void) { return 0; }"
import sys
if len(sys.argv) > 1:
    f = open(sys.argv[1])
    fn = sys.argv[1]
else:
    f = sys.stdin
    fn = "<stdin>"
f = f.read()
a, b = f.split("\n;;\n", 1)

p = parse('File', a, fn)
if not p: raise SystemExit, 1

if len(sys.argv) > 2:
    f = open(sys.argv[2], "w")
else:
    f = sys.stdout

prologue(f)
f.write(b)
epilogue(f)
