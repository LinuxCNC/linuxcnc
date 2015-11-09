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

import os, sys, tempfile, shutil, getopt, time
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

%%
parser Hal:
    ignore: "//.*"
    ignore: "/[*](.|\n)*?[*]/"
    ignore: "[ \t\r\n]+"

    token END: ";;"
    token PARAMDIRECTION: "rw|r"
    token PINDIRECTION: "in|out|io"
    token TYPE: "float|bit|signed|unsigned|u32|s32"
    token NAME: "[a-zA-Z_][a-zA-Z0-9_]*"
    token STARREDNAME: "[*]*[a-zA-Z_][a-zA-Z0-9_]*"
    token HALNAME: "[#a-zA-Z_][-#a-zA-Z0-9_.]*"
    token FPNUMBER: "-?([0-9]*\.[0-9]+|[0-9]+\.?)([Ee][+-]?[0-9]+)?f?"
    token NUMBER: "0x[0-9a-fA-F]+|[+-]?[0-9]+"
    token STRING: "\"(\\.|[^\\\"])*\""
    token HEADER: "<.*?>"
    token POP: "[-()+*/]|&&|\\|\\||personality|==|&|!=|<|<=|>|>="
    token TSTRING: "\"\"\"(\\.|\\\n|[^\\\"]|\"(?!\"\")|\n)*\"\"\""

    rule File: ComponentDeclaration Declaration* "$" {{ return True }}
    rule ComponentDeclaration:
        "component" NAME OptString";" {{ comp(NAME, OptString); }}
    rule Declaration:
        "pin" PINDIRECTION TYPE HALNAME OptArray OptSAssign OptPersonality OptString ";"  {{ pin(HALNAME, TYPE, OptArray, PINDIRECTION, OptString, OptSAssign, OptPersonality) }}
      | "param" PARAMDIRECTION TYPE HALNAME OptArray OptSAssign OptPersonality OptString ";" {{ param(HALNAME, TYPE, OptArray, PARAMDIRECTION, OptString, OptSAssign, OptPersonality) }}
      | "function" NAME OptFP OptString ";"       {{ function(NAME, OptFP, OptString) }}
      | "variable" NAME STARREDNAME OptSimpleArray OptAssign ";" {{ variable(NAME, STARREDNAME, OptSimpleArray, OptAssign) }}
      | "option" NAME OptValue ";"   {{ option(NAME, OptValue) }}
      | "see_also" String ";"   {{ see_also(String) }}
      | "notes" String ";"   {{ notes(String) }}
      | "description" String ";"   {{ description(String) }}
      | "license" String ";"   {{ license(String) }}
      | "author" String ";"   {{ author(String) }}
      | "include" Header ";"   {{ include(Header) }}
      | "modparam" NAME {{ NAME1=NAME; }} NAME OptSAssign OptString ";" {{ modparam(NAME1, NAME, OptSAssign, OptString) }}

    rule Header: STRING {{ return STRING }} | HEADER {{ return HEADER }}

    rule String: TSTRING {{ return eval(TSTRING) }}
            | STRING {{ return eval(STRING) }}

    rule OptPersonality: "if" Personality {{ return Personality }}
            | {{ return None }}
    rule Personality: {{ pp = [] }} (PersonalityPart {{ pp.append(PersonalityPart) }} )* {{ return " ".join(pp) }}
    rule PersonalityPart: NUMBER {{ return NUMBER }}
            | POP {{ return POP }}
    rule OptSimpleArray: "\[" NUMBER "\]" {{ return int(NUMBER) }}
            | {{ return 0 }}
    rule OptArray: "\[" NUMBER OptArrayPersonality "\]" {{ return OptArrayPersonality and (int(NUMBER), OptArrayPersonality) or int(NUMBER) }}
            | {{ return 0 }}
    rule OptArrayPersonality: ":" Personality {{ return Personality }}
            | {{ return None }}
    rule OptString: TSTRING {{ return eval(TSTRING) }}
            | STRING {{ return eval(STRING) }}
            | {{ return '' }}
    rule OptAssign: "=" Value {{ return Value; }}
                | {{ return None }}
    rule OptSAssign: "=" SValue {{ return SValue; }}
                | {{ return None }}
    rule OptFP: "fp" {{ return 1 }} | "nofp" {{ return 0 }} | {{ return 1 }}
    rule Value: "yes" {{ return 1 }} | "no" {{ return 0 }}
                | "true" {{ return 1 }} | "false" {{ return 0 }}
                | "TRUE" {{ return 1 }} | "FALSE" {{ return 0 }}
                | NAME {{ return NAME }}
                | FPNUMBER {{ return float(FPNUMBER.rstrip("f")) }}
                | NUMBER {{ return int(NUMBER,0) }}
    rule SValue: "yes" {{ return "yes" }} | "no" {{ return "no" }}
                | "true" {{ return "true" }} | "false" {{ return "false" }}
                | "TRUE" {{ return "TRUE" }} | "FALSE" {{ return "FALSE" }}
                | NAME {{ return NAME }}
                | FPNUMBER {{ return FPNUMBER }}
                | NUMBER {{ return NUMBER }}
    rule OptValue: Value {{ return Value }}
                | {{ return 1 }}
    rule OptSValue: SValue {{ return SValue }}
                | {{ return 1 }}
%%

mp_decl_map = {'int': 'RTAPI_MP_INT', 'dummy': None}

# These are symbols that comp puts in the global namespace of the C file it
# creates.  The user is thus not allowed to add any symbols with these
# names.  That includes not only global variables and functions, but also
# HAL pins & parameters, because comp adds #defines with the names of HAL
# pins & params.
reserved_names = [ 'comp_id', 'fperiod', 'rtapi_app_main', 'rtapi_app_exit', 'extra_setup', 'extra_cleanup' ]

def _parse(rule, text, filename=None):
    global P, S
    S = HalScanner(text, filename=filename)
    P = Hal(S)
    return runtime.wrap_error_reporter(P, rule)

def parse(filename):
    initialize()
    f = open(filename).read()
    a, b = f.split("\n;;\n", 1)
    p = _parse('File', a + "\n\n", filename)
    if not p: raise SystemExit, 1
    if require_license:
        if not finddoc('license'):
            raise SystemExit, "Error: %s:0: License not specified" % filename
    return a, b

dirmap = {'r': 'HAL_RO', 'rw': 'HAL_RW', 'in': 'HAL_IN', 'out': 'HAL_OUT', 'io': 'HAL_IO' }
typemap = {'signed': 's32', 'unsigned': 'u32'}
deprmap = {'s32': 'signed', 'u32': 'unsigned'}
deprecated = ['s32', 'u32']

def initialize():
    global functions, params, pins, options, comp_name, names, docs, variables
    global modparams, includes

    functions = []; params = []; pins = []; options = {}; variables = []
    modparams = []; docs = []; includes = [];
    comp_name = None

    names = {}

def Warn(msg, *args):
    if args:
        msg = msg % args
    print >>sys.stderr, "%s:%d: Warning: %s" % (S.filename, S.line, msg)

def Error(msg, *args):
    if args:
        msg = msg % args
    raise runtime.SyntaxError(S.get_pos(), msg, None)

def comp(name, doc):
    docs.append(('component', name, doc))
    global comp_name
    if comp_name:
        Error("Duplicate specification of component name")
    comp_name = name;

def description(doc):
    docs.append(('descr', doc));

def license(doc):
    docs.append(('license', doc));

def author(doc):
    docs.append(('author', doc));

def see_also(doc):
    docs.append(('see_also', doc));

def notes(doc):
    docs.append(('notes', doc));

def type2type(type):
    # When we start warning about s32/u32 this is where the warning goes
    return typemap.get(type, type)

def checkarray(name, array):
    hashes = len(re.findall("#+", name))
    if array:
        if hashes == 0: Error("Array name contains no #: %r" % name)
        if hashes > 1: Error("Array name contains more than one block of #: %r" % name)
    else:
        if hashes > 0: Error("Non-array name contains #: %r" % name)

def check_name_ok(name):
    if name in reserved_names:
        Error("Variable name %s is reserved" % name)
    if name in names:
        Error("Duplicate item name %s" % name)

def pin(name, type, array, dir, doc, value, personality):
    checkarray(name, array)
    type = type2type(type)
    check_name_ok(name)
    docs.append(('pin', name, type, array, dir, doc, value, personality))
    names[name] = None
    pins.append((name, type, array, dir, value, personality))

def param(name, type, array, dir, doc, value, personality):
    checkarray(name, array)
    type = type2type(type)
    check_name_ok(name)
    docs.append(('param', name, type, array, dir, doc, value, personality))
    names[name] = None
    params.append((name, type, array, dir, value, personality))

def function(name, fp, doc):
    check_name_ok(name)
    docs.append(('funct', name, fp, doc))
    names[name] = None
    functions.append((name, fp))

def option(name, value):
    if name in options:
        Error("Duplicate option name %s" % name)
    options[name] = value

def variable(type, name, array, default):
    check_name_ok(name)
    names[name] = None
    variables.append((type, name, array, default))

def modparam(type, name, default, doc):
    check_name_ok(name)
    names[name] = None
    modparams.append((type, name, default, doc))

def include(value):
    includes.append((value))

def removeprefix(s,p):
    if s.startswith(p): return s[len(p):]
    return s

def to_hal(name):
    name = re.sub("#+", lambda m: "%%0%dd" % len(m.group(0)), name)
    return name.replace("_", "-").rstrip("-").rstrip(".")
def to_c(name):
    name = re.sub("[-._]*#+", "", name)
    name = name.replace("#", "").replace(".", "_").replace("-", "_")
    return re.sub("_+", "_", name)

def prologue(f):
    print >> f, "/* Autogenerated by %s -- do not edit */" % (
        sys.argv[0])
    print >> f, """\
#include "rtapi.h"
#ifdef RTAPI
#include "rtapi_app.h"
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"

static int comp_id;
"""
    for name in includes:
        print >>f, "#include %s" % name

    names = {}

    def q(s):
        s = s.replace("\\", "\\\\")
        s = s.replace("\"", "\\\"")
        s = s.replace("\r", "\\r")
        s = s.replace("\n", "\\n")
        s = s.replace("\t", "\\t")
        s = s.replace("\v", "\\v")
        return '"%s"' % s

    print >>f, "#ifdef MODULE_INFO"
    for v in docs:
        if not v: continue
        v = ":".join(map(str, v))
        print >>f, "MODULE_INFO(linuxcnc, %s);" % q(v)
        license = finddoc('license')
    if license and license[1]:
        print >>f, "MODULE_LICENSE(\"%s\");" % license[1].split("\n")[0]
    print >>f, "#endif // MODULE_INFO"
    print >>f


    has_data = options.get("data")

    has_array = False
    has_personality = False
    for name, type, array, dir, value, personality in pins:
        if array: has_array = True
        if isinstance(array, tuple): has_personality = True
        if personality: has_personality = True
    for name, type, array, dir, value, personality in params:
        if array: has_array = True
        if isinstance(array, tuple): has_personality = True
        if personality: has_personality = True
    for type, name, default, doc in modparams:
        decl = mp_decl_map[type]
        if decl:
            print >>f, "%s %s" % (type, name),
            if default: print >>f, "= %s;" % default
            else: print >>f, ";"
            print >>f, "%s(%s, %s);" % (decl, name, q(doc))

    print >>f
    print >>f, "struct __comp_state {"
    print >>f, "    struct __comp_state *_next;"
    if has_personality:
        print >>f, "    int _personality;"

    for name, type, array, dir, value, personality in pins:
        if array:
            if isinstance(array, tuple): array = array[0]
            print >>f, "    hal_%s_t *%s[%s];" % (type, to_c(name), array)
        else:
            print >>f, "    hal_%s_t *%s;" % (type, to_c(name))
        names[name] = 1

    for name, type, array, dir, value, personality in params:
        if array:
            if isinstance(array, tuple): array = array[0]
            print >>f, "    hal_%s_t %s[%s];" % (type, to_c(name), array)
        else:
            print >>f, "    hal_%s_t %s;" % (type, to_c(name))
        names[name] = 1

    for type, name, array, value in variables:
        if array:
            print >>f, "    %s %s[%d];\n" % (type, name, array)
        else:
            print >>f, "    %s %s;\n" % (type, name)
    if has_data:
        print >>f, "    void *_data;"

    print >>f, "};"

    if options.get("userspace"):
        print >>f, "#include <stdlib.h>"

    print >>f, "struct __comp_state *__comp_inst=0;"
    print >>f, "struct __comp_state *__comp_first_inst=0, *__comp_last_inst=0;"

    print >>f
    for name, fp in functions:
        if names.has_key(name):
            Error("Duplicate item name: %s" % name)
        print >>f, "static void %s(struct __comp_state *__comp_inst, long period);" % to_c(name)
        names[name] = 1

    print >>f, "static int __comp_get_data_size(void);"
    if options.get("extra_setup"):
        print >>f, "static int extra_setup(struct __comp_state *__comp_inst, char *prefix, long extra_arg);"
    if options.get("extra_cleanup"):
        print >>f, "static void extra_cleanup(void);"

    if not options.get("no_convenience_defines"):
        print >>f, "#undef TRUE"
        print >>f, "#define TRUE (1)"
        print >>f, "#undef FALSE"
        print >>f, "#define FALSE (0)"
        print >>f, "#undef true"
        print >>f, "#define true (1)"
        print >>f, "#undef false"
        print >>f, "#define false (0)"

    print >>f
    if has_personality:
        print >>f, "static int export(char *prefix, long extra_arg, long personality) {"
    else:
        print >>f, "static int export(char *prefix, long extra_arg) {"
    if len(functions) > 0:
        print >>f, "    char buf[HAL_NAME_LEN + 1];"
    print >>f, "    int r = 0;"
    if has_array:
        print >>f, "    int j = 0;"
    print >>f, "    int sz = sizeof(struct __comp_state) + __comp_get_data_size();"
    print >>f, "    struct __comp_state *inst = hal_malloc(sz);"
    print >>f, "    memset(inst, 0, sz);"
    if has_data:
        print >>f, "    inst->_data = (char*)inst + sizeof(struct __comp_state);"
    if has_personality:
        print >>f, "    inst->_personality = personality;"
    if options.get("extra_setup"):
        print >>f, "    r = extra_setup(inst, prefix, extra_arg);"
	print >>f, "    if(r != 0) return r;"
        # the extra_setup() function may have changed the personality
        if has_personality:
            print >>f, "    personality = inst->_personality;"
    for name, type, array, dir, value, personality in pins:
        if personality:
            print >>f, "if(%s) {" % personality
        if array:
            if isinstance(array, tuple): array = array[1]
            print >>f, "    for(j=0; j < (%s); j++) {" % array
            print >>f, "        r = hal_pin_%s_newf(%s, &(inst->%s[j]), comp_id," % (
                type, dirmap[dir], to_c(name))
            print >>f, "            \"%%s%s\", prefix, j);" % to_hal("." + name)
            print >>f, "        if(r != 0) return r;"
            if value is not None:
                print >>f, "    *(inst->%s[j]) = %s;" % (to_c(name), value)
            print >>f, "    }"
        else:
            print >>f, "    r = hal_pin_%s_newf(%s, &(inst->%s), comp_id," % (
                type, dirmap[dir], to_c(name))
            print >>f, "        \"%%s%s\", prefix);" % to_hal("." + name)
            print >>f, "    if(r != 0) return r;"
            if value is not None:
                print >>f, "    *(inst->%s) = %s;" % (to_c(name), value)
        if personality:
            print >>f, "}"

    for name, type, array, dir, value, personality in params:
        if personality:
            print >>f, "if(%s) {" % personality
        if array:
            if isinstance(array, tuple): array = array[1]
            print >>f, "    for(j=0; j < %s; j++) {" % array
            print >>f, "        r = hal_param_%s_newf(%s, &(inst->%s[j]), comp_id," % (
                type, dirmap[dir], to_c(name))
            print >>f, "            \"%%s%s\", prefix, j);" % to_hal("." + name)
            print >>f, "        if(r != 0) return r;"
            if value is not None:
                print >>f, "    inst->%s[j] = %s;" % (to_c(name), value)
            print >>f, "    }"
        else:
            print >>f, "    r = hal_param_%s_newf(%s, &(inst->%s), comp_id," % (
                type, dirmap[dir], to_c(name))
            print >>f, "        \"%%s%s\", prefix);" % to_hal("." + name)
            if value is not None:
                print >>f, "    inst->%s = %s;" % (to_c(name), value)
            print >>f, "    if(r != 0) return r;"
        if personality:
            print >>f, "}"

    for type, name, array, value in variables:
        if value is None: continue
        if array:
            print >>f, "    for(j=0; j < %s; j++) {" % array
            print >>f, "        inst->%s[j] = %s;" % (name, value)
            print >>f, "    }"
        else:
            print >>f, "    inst->%s = %s;" % (name, value)

    for name, fp in functions:
        print >>f, "    rtapi_snprintf(buf, sizeof(buf), \"%%s%s\", prefix);"\
            % to_hal("." + name)
        print >>f, "    r = hal_export_funct(buf, (void(*)(void *inst, long))%s, inst, %s, 0, comp_id);" % (
            to_c(name), int(fp))
        print >>f, "    if(r != 0) return r;"
    print >>f, "    if(__comp_last_inst) __comp_last_inst->_next = inst;"
    print >>f, "    __comp_last_inst = inst;"
    print >>f, "    if(!__comp_first_inst) __comp_first_inst = inst;"
    print >>f, "    return 0;"
    print >>f, "}"

    if options.get("count_function"):
        print >>f, "static int get_count(void);"

    if options.get("rtapi_app", 1):
        if options.get("constructable") and not options.get("singleton"):
            print >>f, "static int export_1(char *prefix, char *argstr) {"
            print >>f, "    int arg = simple_strtol(argstr, NULL, 0);"
            print >>f, "    return export(prefix, arg);"
            print >>f, "}"
        if not options.get("singleton") and not options.get("count_function") :
            print >>f, "static int default_count=%s, count=0;" \
                % options.get("default_count", 1)
            print >>f, "char *names[16] = {0,};"
            if not options.get("userspace"):
                print >>f, "RTAPI_MP_INT(count, \"number of %s\");" % comp_name
                print >>f, "RTAPI_MP_ARRAY_STRING(names, 16, \"names of %s\");" % comp_name

        if has_personality:
            init1 = str(int(options.get('default_personality', 0)))
            init = ",".join([init1] * 16)
            print >>f, "static int personality[16] = {%s};" % init
            print >>f, "RTAPI_MP_ARRAY_INT(personality, 16, \"personality of each %s\");" % comp_name
        print >>f, "int rtapi_app_main(void) {"
        print >>f, "    int r = 0;"
        if not options.get("singleton"):
            print >>f, "    int i;"
        if options.get("count_function"):
            print >>f, "    int count = get_count();"

        print >>f, "    comp_id = hal_init(\"%s\");" % comp_name
        print >>f, "    if(comp_id < 0) return comp_id;"

        if options.get("singleton"):
            if has_personality:
                print >>f, "    r = export(\"%s\", 0, personality[0]);" % \
                        to_hal(removeprefix(comp_name, "hal_"))
            else:
                print >>f, "    r = export(\"%s\", 0);" % \
                        to_hal(removeprefix(comp_name, "hal_"))
        elif options.get("count_function"):
            print >>f, "    for(i=0; i<count; i++) {"
            print >>f, "        char buf[HAL_NAME_LEN + 1];"
            print >>f, "        rtapi_snprintf(buf, sizeof(buf), " \
                                        "\"%s.%%d\", i);" % \
                    to_hal(removeprefix(comp_name, "hal_"))
            if has_personality:
                print >>f, "        r = export(buf, i, personality[i%16]);"
            else:
                print >>f, "        r = export(buf, i);"
	    print >>f, "    }"
	else:
            print >>f, "    if(count && names[0]) {"
            print >>f, "        rtapi_print_msg(RTAPI_MSG_ERR," \
                            "\"count= and names= are mutually exclusive\\n\");"
            print >>f, "        return -EINVAL;"
            print >>f, "    }"
            print >>f, "    if(!count && !names[0]) count = default_count;"
            print >>f, "    if(count) {"
            print >>f, "        for(i=0; i<count; i++) {"
            print >>f, "            char buf[HAL_NAME_LEN + 1];"
            print >>f, "            rtapi_snprintf(buf, sizeof(buf), " \
                                        "\"%s.%%d\", i);" % \
                    to_hal(removeprefix(comp_name, "hal_"))
            if has_personality:
                print >>f, "        r = export(buf, i, personality[i%16]);"
            else:
                print >>f, "        r = export(buf, i);"
            print >>f, "            if(r != 0) break;"
            print >>f, "       }"
            print >>f, "    } else {"
            print >>f, "        for(i=0; names[i]; i++) {"
            if has_personality:
                print >>f, "        r = export(names[i], i, personality[i%16]);"
            else:
                print >>f, "        r = export(names[i], i);"
            print >>f, "            if(r != 0) break;"
            print >>f, "       }"
            print >>f, "    }"

        if options.get("constructable") and not options.get("singleton"):
            print >>f, "    hal_set_constructor(comp_id, export_1);"
        print >>f, "    if(r) {"
	if options.get("extra_cleanup"):
            print >>f, "    extra_cleanup();"
        print >>f, "        hal_exit(comp_id);"
        print >>f, "    } else {"
        print >>f, "        hal_ready(comp_id);"
        print >>f, "    }"
        print >>f, "    return r;"
        print >>f, "}"

        print >>f
        print >>f, "void rtapi_app_exit(void) {"
	if options.get("extra_cleanup"):
            print >>f, "    extra_cleanup();"
        print >>f, "    hal_exit(comp_id);"
        print >>f, "}"

    if options.get("userspace"):
        print >>f, "static void user_mainloop(void);"
        if options.get("userinit"):
            print >>f, "static void userinit(int argc, char **argv);"
        print >>f, "int argc=0; char **argv=0;"
        print >>f, "int main(int argc_, char **argv_) {"
        print >>f, "    argc = argc_; argv = argv_;"
        print >>f
        if options.get("userinit", 0):
            print >>f, "    userinit(argc, argv);"
        print >>f
        print >>f, "    if(rtapi_app_main() < 0) return 1;"
        print >>f, "    user_mainloop();"
        print >>f, "    rtapi_app_exit();"
        print >>f, "    return 0;"
        print >>f, "}"

    print >>f
    if not options.get("no_convenience_defines"):
        print >>f, "#undef FUNCTION"
        print >>f, "#define FUNCTION(name) static void name(struct __comp_state *__comp_inst, long period)"
        print >>f, "#undef EXTRA_SETUP"
        print >>f, "#define EXTRA_SETUP() static int extra_setup(struct __comp_state *__comp_inst, char *prefix, long extra_arg)"
        print >>f, "#undef EXTRA_CLEANUP"
        print >>f, "#define EXTRA_CLEANUP() static void extra_cleanup(void)"
        print >>f, "#undef fperiod"
        print >>f, "#define fperiod (period * 1e-9)"
        for name, type, array, dir, value, personality in pins:
            print >>f, "#undef %s" % to_c(name)
            if array:
                if dir == 'in':
                    print >>f, "#define %s(i) (0+*(__comp_inst->%s[i]))" % (to_c(name), to_c(name))
                else:
                    print >>f, "#define %s(i) (*(__comp_inst->%s[i]))" % (to_c(name), to_c(name))
            else:
                if dir == 'in':
                    print >>f, "#define %s (0+*__comp_inst->%s)" % (to_c(name), to_c(name))
                else:
                    print >>f, "#define %s (*__comp_inst->%s)" % (to_c(name), to_c(name))
        for name, type, array, dir, value, personality in params:
            print >>f, "#undef %s" % to_c(name)
            if array:
                print >>f, "#define %s(i) (__comp_inst->%s[i])" % (to_c(name), to_c(name))
            else:
                print >>f, "#define %s (__comp_inst->%s)" % (to_c(name), to_c(name))

        for type, name, array, value in variables:
            name = name.replace("*", "")
            print >>f, "#undef %s" % name
            print >>f, "#define %s (__comp_inst->%s)" % (name, name)

        if has_data:
            print >>f, "#undef data"
            print >>f, "#define data (*(%s*)(__comp_inst->_data))" % options['data']
        if has_personality:
            print >>f, "#undef personality"
            print >>f, "#define personality (__comp_inst->_personality)"

        if options.get("userspace"):
            print >>f, "#undef FOR_ALL_INSTS"
            print >>f, "#define FOR_ALL_INSTS() for(__comp_inst = __comp_first_inst; __comp_inst; __comp_inst = __comp_inst->_next)"
    print >>f
    print >>f

def epilogue(f):
    data = options.get('data')
    print >>f
    if data:
        print >>f, "static int __comp_get_data_size(void) { return sizeof(%s); }" % data
    else:
        print >>f, "static int __comp_get_data_size(void) { return 0; }"

INSTALL, COMPILE, PREPROCESS, DOCUMENT, INSTALLDOC, VIEWDOC, MODINC = range(7)
modename = ("install", "compile", "preprocess", "document", "installdoc", "viewdoc", "print-modinc")

modinc = None
def find_modinc():
    global modinc
    if modinc: return modinc
    d = os.path.abspath(os.path.dirname(os.path.dirname(sys.argv[0])))
    for e in ['src', 'etc/linuxcnc', '/etc/linuxcnc', 'share/linuxcnc']:
        e = os.path.join(d, e, 'Makefile.modinc')
        if os.path.exists(e):
            modinc = e
            return e
    raise SystemExit, "Error: Unable to locate Makefile.modinc"

def build_usr(tempdir, filename, mode, origfilename):
    binname = os.path.basename(os.path.splitext(filename)[0])

    makefile = os.path.join(tempdir, "Makefile")
    f = open(makefile, "w")
    print >>f, "%s: %s" % (binname, filename)
    print >>f, "\t$(CC) $(EXTRA_CFLAGS) -URTAPI -U__MODULE__ -DULAPI -Os %s -o $@ $< -Wl,-rpath,$(LIBDIR) -L$(LIBDIR) -llinuxcnchal %s" % (
        options.get("extra_compile_args", ""),
        options.get("extra_link_args", ""))
    print >>f, "include %s" % find_modinc()
    f.close()
    result = os.system("cd %s && make -S %s" % (tempdir, binname))
    if result != 0:
        raise SystemExit, os.WEXITSTATUS(result) or 1
    output = os.path.join(tempdir, binname)
    if mode == INSTALL:
        shutil.copy(output, os.path.join(BASE, "bin", binname))
    elif mode == COMPILE:
        shutil.copy(output, os.path.join(os.path.dirname(origfilename),binname))

def build_rt(tempdir, filename, mode, origfilename):
    objname = os.path.basename(os.path.splitext(filename)[0] + ".o")
    makefile = os.path.join(tempdir, "Makefile")
    f = open(makefile, "w")
    print >>f, "obj-m += %s" % objname
    print >>f, "include %s" % find_modinc()
    print >>f, "EXTRA_CFLAGS += -I%s" % os.path.abspath(os.path.dirname(origfilename))
    print >>f, "EXTRA_CFLAGS += -I%s" % os.path.abspath('.')
    f.close()
    if mode == INSTALL:
        target = "modules install"
    else:
        target = "modules"
    result = os.system("cd %s && make -S %s" % (tempdir, target))
    if result != 0:
        raise SystemExit, os.WEXITSTATUS(result) or 1
    if mode == COMPILE:
        for extension in ".ko", ".so", ".o":
            kobjname = os.path.splitext(filename)[0] + extension
            if os.path.exists(kobjname):
                shutil.copy(kobjname, os.path.basename(kobjname))
                break
        else:
            raise SystemExit, "Error: Unable to copy module from temporary directory"

def finddoc(section=None, name=None):
    for item in docs:
        if ((section == None or section == item[0]) and
                (name == None or name == item[1])): return item
    return None

def finddocs(section=None, name=None):
    for item in docs:
        if ((section == None or section == item[0]) and
                (name == None or name == item[1])):
                    yield item

def to_hal_man_unnumbered(s):
    s = "%s.%s" % (comp_name, s)
    s = s.replace("_", "-")
    s = s.rstrip("-")
    s = s.rstrip(".")
    s = re.sub("#+", lambda m: "\\fI" + "M" * len(m.group(0)) + "\\fB", s)
    # s = s.replace("-", "\\-")
    return s


def to_hal_man(s):
    if options.get("singleton"):
        s = "%s.%s" % (comp_name, s)
    else:
        s = "%s.\\fIN\\fB.%s" % (comp_name, s)
    s = s.replace("_", "-")
    s = s.rstrip("-")
    s = s.rstrip(".")
    s = re.sub("#+", lambda m: "\\fI" + "M" * len(m.group(0)) + "\\fB", s)
    # s = s.replace("-", "\\-")
    return s

def document(filename, outfilename):
    if outfilename is None:
        outfilename = os.path.splitext(filename)[0] + ".9comp"

    a, b = parse(filename)
    f = open(outfilename, "w")

    has_personality = False
    for name, type, array, dir, value, personality in pins:
        if personality: has_personality = True
        if isinstance(array, tuple): has_personality = True
    for name, type, array, dir, value, personality in params:
        if personality: has_personality = True
        if isinstance(array, tuple): has_personality = True

    print >>f, ".TH %s \"9\" \"%s\" \"LinuxCNC Documentation\" \"HAL Component\"" % (
        comp_name.upper(), time.strftime("%F"))
    print >>f, ".de TQ\n.br\n.ns\n.TP \\\\$1\n..\n"

    print >>f, ".SH NAME\n"
    doc = finddoc('component')
    if doc and doc[2]:
        if '\n' in doc[2]:
            firstline, rest = doc[2].split('\n', 1)
        else:
            firstline = doc[2]
            rest = ''
        print >>f, "%s \\- %s" % (doc[1], firstline)
    else:
        rest = ''
        print >>f, "%s" % doc[1]


    print >>f, ".SH SYNOPSIS"
    if options.get("userspace"):
        print >>f, ".B %s" % comp_name
    else:
        if rest:
            print >>f, rest
        else:
            print >>f, ".HP"
            if options.get("singleton") or options.get("count_function"):
                if has_personality:
                    print >>f, ".B loadrt %s personality=\\fIP\\fB" % comp_name,
                else:
                    print >>f, ".B loadrt %s" % comp_name,
            else:
                if has_personality:
                    print >>f, ".B loadrt %s [count=\\fIN\\fB|names=\\fIname1\\fB[,\\fIname2...\\fB]] [personality=\\fIP,P,...\\fB]" % comp_name,
                else:
                    print >>f, ".B loadrt %s [count=\\fIN\\fB|names=\\fIname1\\fB[,\\fIname2...\\fB]]" % comp_name,
            for type, name, default, doc in modparams:
                print >>f, "[%s=\\fIN\\fB]" % name,
            print >>f

            hasparamdoc = False
            for type, name, default, doc in modparams:
                if doc: hasparamdoc = True

            if hasparamdoc:
                print >>f, ".RS 4"
                for type, name, default, doc in modparams:
                    print >>f, ".TP"
                    print >>f, "\\fB%s\\fR" % name,
                    if default:
                        print >>f, "[default: %s]" % default
                    else:
                        print >>f
                    print >>f, doc
                print >>f, ".RE"

        if options.get("constructable") and not options.get("singleton"):
            print >>f, ".PP\n.B newinst %s \\fIname\\fB" % comp_name

    doc = finddoc('descr')
    if doc and doc[1]:
        print >>f, ".SH DESCRIPTION\n"
        print >>f, "%s" % doc[1]

    if functions:
        print >>f, ".SH FUNCTIONS"
        for _, name, fp, doc in finddocs('funct'):
            print >>f, ".TP"
            print >>f, "\\fB%s\\fR" % to_hal_man(name),
            if fp:
                print >>f, "(requires a floating-point thread)"
            else:
                print >>f
            print >>f, doc

    lead = ".TP"
    print >>f, ".SH PINS"
    for _, name, type, array, dir, doc, value, personality in finddocs('pin'):
        print >>f, lead
        print >>f, ".B %s\\fR" % to_hal_man(name),
        print >>f, type, dir,
        if array:
            sz = name.count("#")
            if isinstance(array, tuple):
                print >>f, " (%s=%0*d..%s)" % ("M" * sz, sz, 0, array[1]),
            else:
                print >>f, " (%s=%0*d..%0*d)" % ("M" * sz, sz, 0, sz, array-1),
        if personality:
            print >>f, " [if %s]" % personality,
        if value:
            print >>f, "\\fR(default: \\fI%s\\fR)" % value
        else:
            print >>f, "\\fR"
        if doc:
            print >>f, doc
            lead = ".TP"
        else:
            lead = ".TQ"

    lead = ".TP"
    if params:
        print >>f, ".SH PARAMETERS"
        for _, name, type, array, dir, doc, value, personality in finddocs('param'):
            print >>f, lead
            print >>f, ".B %s\\fR" % to_hal_man(name),
            print >>f, type, dir,
            if array:
                sz = name.count("#")
                if isinstance(array, tuple):
                    print >>f, " (%s=%0*d..%s)" % ("M" * sz, sz, 0, array[1]),
                else:
                    print >>f, " (%s=%0*d..%0*d)" % ("M" * sz, sz, 0, sz, array-1),
            if personality:
                print >>f, " [if %s]" % personality,
            if value:
                print >>f, "\\fR(default: \\fI%s\\fR)" % value
            else:
                print >>f, "\\fR"
            if doc:
                print >>f, doc
                lead = ".TP"
            else:
                lead = ".TQ"

    doc = finddoc('see_also')
    if doc and doc[1]:
        print >>f, ".SH SEE ALSO\n"
        print >>f, "%s" % doc[1]

    doc = finddoc('notes')
    if doc and doc[1]:
        print >>f, ".SH NOTES\n"
        print >>f, "%s" % doc[1]

    doc = finddoc('author')
    if doc and doc[1]:
        print >>f, ".SH AUTHOR\n"
        print >>f, "%s" % doc[1]

    doc = finddoc('license')
    if doc and doc[1]:
        print >>f, ".SH LICENSE\n"
        print >>f, "%s" % doc[1]

def process(filename, mode, outfilename):
    tempdir = tempfile.mkdtemp()
    try:
        if outfilename is None:
            if mode == PREPROCESS:
                outfilename = os.path.splitext(filename)[0] + ".c"
            else:
                outfilename = os.path.join(tempdir,
                    os.path.splitext(os.path.basename(filename))[0] + ".c")

        a, b = parse(filename)
        base_name = os.path.splitext(os.path.basename(outfilename))[0]
        if comp_name != base_name:
            raise SystemExit, "Error: Component name (%s) does not match filename (%s)" % (comp_name, base_name)
        f = open(outfilename, "w")

        if options.get("userinit") and not options.get("userspace"):
            print >> sys.stderr, "Warning: comp '%s' sets 'userinit' without 'userspace', ignoring" % filename

        if options.get("userspace"):
            if functions:
                raise SystemExit, "Error: Userspace components may not have functions"
        if not pins:
            raise SystemExit, "Error: Component must have at least one pin"
        prologue(f)
        lineno = a.count("\n") + 3

        if options.get("userspace"):
            if functions:
                raise SystemExit, "Error: May not specify functions with a userspace component."
            f.write("#line %d \"%s\"\n" % (lineno, filename))
            f.write(b)
        else:
            if not functions or "FUNCTION" in b:
                f.write("#line %d \"%s\"\n" % (lineno, filename))
                f.write(b)
            elif len(functions) == 1:
                f.write("FUNCTION(%s) {\n" % functions[0][0])
                f.write("#line %d \"%s\"\n" % (lineno, filename))
                f.write(b)
                f.write("}\n")
            else:
                raise SystemExit, "Error: Must use FUNCTION() when more than one function is defined"
        epilogue(f)
        f.close()

        if mode != PREPROCESS:
            if options.get("userspace"):
                build_usr(tempdir, outfilename, mode, filename)
            else:
                build_rt(tempdir, outfilename, mode, filename)

    finally:
        shutil.rmtree(tempdir)

def usage(exitval=0):
    print """%(name)s: Build, compile, and install LinuxCNC HAL components

Usage:
           %(name)s [--compile|--preprocess|--document|--view-doc] compfile...
    [sudo] %(name)s [--install|--install-doc] compfile...
           %(name)s --compile --userspace cfile...
    [sudo] %(name)s --install --userspace cfile...
    [sudo] %(name)s --install --userspace pyfile...
           %(name)s --print-modinc
""" % {'name': os.path.basename(sys.argv[0])}
    raise SystemExit, exitval

def main():
    global require_license
    require_license = True
    mode = PREPROCESS
    outfile = None
    userspace = False
    try:
        opts, args = getopt.getopt(sys.argv[1:], "luijcpdo:h?",
                           ['install', 'compile', 'preprocess', 'outfile=',
                            'document', 'help', 'userspace', 'install-doc',
                            'view-doc', 'require-license', 'print-modinc'])
    except getopt.GetoptError:
        usage(1)

    for k, v in opts:
        if k in ("-u", "--userspace"):
            userspace = True
        if k in ("-i", "--install"):
            mode = INSTALL
        if k in ("-c", "--compile"):
            mode = COMPILE
        if k in ("-p", "--preprocess"):
            mode = PREPROCESS
        if k in ("-d", "--document"):
            mode = DOCUMENT
        if k in ("-j", "--install-doc"):
            mode = INSTALLDOC
        if k in ("-j", "--view-doc"):
            mode = VIEWDOC
        if k in ("--print-modinc",):
            mode = MODINC
        if k in ("-l", "--require-license"):
            require_license = True
        if k in ("-o", "--outfile"):
            if len(args) != 1:
                raise SystemExit, "Error: Cannot specify -o with multiple input files"
            outfile = v
        if k in ("-?", "-h", "--help"):
            usage(0)

    if outfile and mode != PREPROCESS and mode != DOCUMENT:
        raise SystemExit, "Error: Can only specify -o when preprocessing or documenting"

    if mode == MODINC:
        if args:
            raise SystemExit, \
                "Error: Can not specify input files when using --print-modinc"
        print find_modinc()
        return 0

    for f in args:
        try:
            basename = os.path.basename(os.path.splitext(f)[0])
            if f.endswith(".comp") and mode == DOCUMENT:
                document(f, outfile)
            elif f.endswith(".comp") and mode == VIEWDOC:
                tempdir = tempfile.mkdtemp()
                try:
                    outfile = os.path.join(tempdir, basename + ".9comp")
                    document(f, outfile)
                    os.spawnvp(os.P_WAIT, "man", ["man", outfile])
                finally:
                    shutil.rmtree(tempdir)
            elif f.endswith(".comp") and mode == INSTALLDOC:
                manpath = os.path.join(BASE, "share/man/man9")
                if not os.path.isdir(manpath):
                    manpath = os.path.join(BASE, "man/man9")
                outfile = os.path.join(manpath, basename + ".9comp")
                print "INSTALLDOC", outfile
                document(f, outfile)
            elif f.endswith(".comp"):
                process(f, mode, outfile)
            elif f.endswith(".py") and mode == INSTALL:
                lines = open(f).readlines()
                if lines[0].startswith("#!"): del lines[0]
                lines[0] = "#!%s\n" % sys.executable
                outfile = os.path.join(BASE, "bin", basename)
                try: os.unlink(outfile)
                except os.error: pass
                open(outfile, "w").writelines(lines)
                os.chmod(outfile, 0555)
            elif f.endswith(".c") and mode != PREPROCESS:
                initialize()
                tempdir = tempfile.mkdtemp()
                try:
                    shutil.copy(f, tempdir)
                    if userspace:
                        build_usr(tempdir, os.path.join(tempdir, os.path.basename(f)), mode, f)
                    else:
                        build_rt(tempdir, os.path.join(tempdir, os.path.basename(f)), mode, f)
                finally:
                    shutil.rmtree(tempdir)
            else:
                raise SystemExit, "Error: Unrecognized file type for mode %s: %r" % (modename[mode], f)
        except:
            ex_type, ex_value, exc_tb = sys.exc_info()
            try:
                os.unlink(outfile)
            except: # os.error:
                pass
            raise ex_type, ex_value, exc_tb
if __name__ == '__main__':
    main()

# vim:sw=4:sts=4:et
