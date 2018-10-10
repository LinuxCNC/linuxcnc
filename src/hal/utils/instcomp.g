#!/usr/bin/python2
#    This is 'instcomp', a tool to write instantiated components for
#    Machinekit
#
#    Based upon comp from Linuxcnc
#    Copyright 2006 Jeff Epler <jepler@unpythonic.net>
#
#    Adapted and rewritten for instantiatable components
#    ArcEye 2015 <arceyeATmgwareDOTcoDOTuk>
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

import os, sys, tempfile, shutil, getopt, time, re
BASE = os.path.abspath(os.path.join(os.path.dirname(sys.argv[0]), ".."))
sys.path.insert(0, os.path.join(BASE, "lib", "python"))

%%
parser Hal:
    ignore: "//.*"
    ignore: "/[*](.|\n)*?[*]/"
    ignore: "[ \t\r\n]+"

    token END: ";;"
    token PINDIRECTION: "in|out|io"
    token TYPE: "float|bit|signed|unsigned|u32|s32|s64|u64"
    token MPTYPE: "int|string|u32"
    token NAME: "[a-zA-Z_][a-zA-Z0-9_]*"
    token STARREDNAME: "[*]*[a-zA-Z_][a-zA-Z0-9_]*"
    token HALNAME: "[#a-zA-Z_][-#a-zA-Z0-9_.]*"
    token FPNUMBER: "-?([0-9]*\.[0-9]+|[0-9]+\.?)([Ee][+-]?[0-9]+)?f?"
    token NUMBER: "0x[0-9a-fA-F]+|[+-]?[0-9]+"
    token STRING: "\"(\\.|[^\\\"])*\""
    token HEADER: "<.*?>"
    token POP: "[-()+*/]|&&|\\|\\||==|&|!=|<|<=|>|>="
    token TSTRING: "\"\"\"(\\.|\\\n|[^\\\"]|\"(?!\"\")|\n)*\"\"\""

    rule File: ComponentDeclaration Declaration* "$" {{ return True }}
    rule ComponentDeclaration:
        "component" NAME OptString";" {{ comp(NAME, OptString); }}
    rule Declaration:
        "pin" PINDIRECTION TYPE HALNAME OptArrayIndex OptSAssign OptString ";"  {{ pin(HALNAME, TYPE, OptArrayIndex, PINDIRECTION, OptString, OptSAssign) }}
## ring left in as placeholder for now
      | "ring" PINDIRECTION HALNAME OptSAssign OptString ";"  {{ ring(HALNAME, PINDIRECTION, OptString, OptSAssign) }}
      | "pin_ptr" PINDIRECTION TYPE HALNAME OptArrayIndex OptSAssign OptString ";"  {{ pin_ptr(HALNAME, TYPE, OptArrayIndex, PINDIRECTION, OptString, OptSAssign) }}
      | "instanceparam" MPTYPE HALNAME OptSAssign OptString ";" {{ instanceparam(HALNAME, MPTYPE, OptString, OptSAssign) }}
      | "moduleparam" MPTYPE HALNAME OptSAssign OptString ";" {{ moduleparam(HALNAME, MPTYPE, OptSAssign, OptString) }}
      | "function" NAME OptFP OptString ";"       {{ function(NAME, OptFP, OptString) }}
      | "variable" NAME STARREDNAME OptArrayIndex OptSAssign OptString ";" {{ variable(NAME, STARREDNAME, OptArrayIndex, OptString, OptSAssign) }}
      | "userdef_type" NAME STARREDNAME OptArrayIndex OptSAssign OptString ";" {{ userdef_type(NAME, STARREDNAME, OptArrayIndex, OptString, OptSAssign) }}
      | "option" NAME OptValue ";"   {{ option(NAME, OptValue) }}
      | "see_also" String ";"   {{ see_also(String) }}
      | "notes" String ";"   {{ notes(String) }}
      | "description" String ";"   {{ description(String) }}
      | "special_format_doc" String ";"   {{ special_format_doc(String) }}
      | "license" String ";"   {{ license(String) }}
      | "author" String ";"   {{ author(String) }}
      | "userdef_include" Header ";"   {{ userdef_include(Header) }}
      | "modparam" NAME {{ NAME1=NAME; }} NAME OptSAssign OptString ";" {{ modparam(NAME1, NAME, OptSAssign, OptString) }}

    rule Header: STRING {{ return STRING }} | HEADER {{ return HEADER }}

    rule String: TSTRING {{ return eval(TSTRING) }}
            | STRING {{ return eval(STRING) }}

    rule OptSimpleArray: "\[" NUMBER "\]" {{ return int(NUMBER) }}
            | {{ return 0 }}

    rule OptArray: "\[" NUMBER "\]" {{ return int(NUMBER) }}
            | {{ return 0 }}

    rule OptArrayIndex: "\[" SValue "\]" {{ return SValue }}
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
                | STRING {{ return STRING}}

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
reserved_names = [ 'comp_id', 'fperiod', 'rtapi_app_main', 'rtapi_app_exit', 'extra_inst_setup', 'extra_inst_cleanup']

def _parse(rule, text, filename=None):
    global P, S
    S = HalScanner(text, filename=filename)
    P = Hal(S)
    return runtime.wrap_error_reporter(P, rule)

def parse(filename):
    initialize()
    f = open(filename).read()
    if '\r' in f:
        raise SystemExit, "Error: Mac or DOS style line endings in file %s" % filename
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

global funct_
funct_ = False
global funct_name
funct_name = ""

def initialize():
    global functions, instanceparams, moduleparams, pins, pin_ptrs, rings, options, comp_name, names, docs, variables, userdef_types
    global modparams, userdef_includes

    functions = []; instanceparams = []; moduleparams = []; pins = []; pin_ptrs = []; rings = []; options = {}; variables = []
    userdef_types = []; modparams = []; docs = []; userdef_includes = [];
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

def special_format_doc(doc):
    docs.append(('sf_doc', doc));

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

def pin(name, type, array, dir, doc, value):
    checkarray(name, array)
    type = type2type(type)
    check_name_ok(name)
    docs.append(('pin', name, type, array, dir, doc, value))
    names[name] = None
    pins.append((name, type, array, dir, value))

def pin_ptr(name, type, array, dir, doc, value):
    checkarray(name, array)
    type = type2type(type)
    check_name_ok(name)
    docs.append(('pin_ptr', name, type, array, dir, doc, value))
    names[name] = None
    pin_ptrs.append((name, type, array, dir, value))

def ring(name, dir, doc, value):
    check_name_ok(name)
    docs.append(('ring', name, doc, value))
    names[name] = None
    rings.append((name, dir, value))

def instanceparam(name, type, doc, value):
    type = type2type(type)
    check_name_ok(name)
    docs.append(('instanceparam', name, type,  doc, value))
    names[name] = None
    instanceparams.append((name, type, doc, value))
##################################################################
##  These are rt module params, there is currently little purpose
##  in their usage, but left in for future options since the base
##  module could be a working rt module if required

def moduleparam(name, type, doc, value):
    type = type2type(type)
    check_name_ok(name)
    docs.append(('moduleparam', name, type, doc, value))
    names[name] = None
    moduleparams.append((name, type, value))

##################################################################

def function(name, fp, doc):
    check_name_ok(name)
    docs.append(('funct', name, fp, doc))
    names[name] = None
    ## debugging feature request that functions be uniquely named
    ## even if default'_' is chosen by author
    if name == '_':
        global funct_
        global funct_name
        funct_name = "%s%s" % (comp_name, name)
        funct_ = True
    functions.append((name, fp))

def option(name, value):
    if name in options:
        Error("Duplicate option name %s" % name)
    options[name] = value

def variable(type, name, array, doc, value):
    check_name_ok(name)
    names[name] = None
    variables.append((type, name, array, value))

def userdef_type(type, name, array, doc, value):
    check_name_ok(name)
    names[name] = None
    userdef_types.append((type, name, array, value))

def modparam(type, name, default, doc):
    check_name_ok(name)
    names[name] = None
    modparams.append((type, name, default, doc))

def userdef_include(value):
    userdef_includes.append((value))

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

def to_noquotes(name):
    name = name.replace("\"", "")
    return name


##################### Start ########################################

def prologue(f):

    if options.get("userspace"):
        raise SystemExit, "Error: instcomp does not support userspace components"

    if not functions :
        raise SystemExit, """\
                        Error: Component code must declare a function.
                        For single functions the default
                        function _; declaration is fine.
                        Multiple functions must be uniquely named.\n"""

    print >> f, "\n/* Autogenerated by %s on %s -- do not edit */\n" % (
        sys.argv[0], time.asctime())


    print >> f, """\
#include "rtapi.h"
#ifdef RTAPI
#include "rtapi_app.h"
#endif
#include "rtapi_string.h"
#include "rtapi_errno.h"
#include "hal.h"
#include "hal_priv.h"
#include "hal_accessor.h"
#include "hal_internal.h"
\nstatic int comp_id;
"""
    for value in userdef_includes:
        print >>f, "#include %s" % value

##  pincount is reserved instanceparam name
##
    global maxpins
    maxpins = 0
    global have_maxpins
    have_maxpins = False

    global numpins
    numpins = 0
    global have_numpins
    have_numpins = False
    # lists of array size specifiers and values
    iplist = []
    ipvlist = []

    global have_count
    have_count = False

    print >>f, "\nstatic char *compname = \"%s\";\n" % (comp_name)

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
        print >>f, "MODULE_INFO(machinekit, %s);" % q(v)
        license = finddoc('license')
    if license and license[1]:
        print >>f, "MODULE_LICENSE(\"%s\");" % license[1].split("\n")[0]
    print >>f, "#endif // MODULE_INFO"

    print >>f, "RTAPI_TAG(HAL,HC_INSTANTIABLE);"
    if options.get("singleton"):
        print "The singleton component is deprecated\njust make sure you only load one if they clash"
    print >>f
    if (pin_ptrs):
        print >>f, "RTAPI_TAG(HAL,HC_SMP_SAFE);"

    has_array = False
    for name, type, array, dir, value in pins:
        if array: has_array = True

    for type, name, default, doc in modparams:
        decl = mp_decl_map[type]
        if decl:
            print >>f, "%s %s" % (type, name),
            if default: print >>f, "= %s;" % default
            else: print >>f, ";"
            print >>f, "%s(%s, %s);" % (decl, name, q(doc))

    print >>f

###  Get the values from the instanceparams ############################################################

    for name, mptype, doc, value in instanceparams:
        if (mptype == 'string'):
            raise SystemExit, """\
                Error: instanceparams of type 'string' are not supported in instcomp

                Kernel parameters were never intended to be used several times
                with different data, as happens with several instances of icomps

                The parameter is a volatile variable and will be overwriten
                by the next instance to use it.

                Special measures were taken to allow the pincount=N int
                instanceparam to function correctly, but this is impractical
                for string instance params, which could contain anything.

                Use the copy of argv (ip->local_argv) and pass strings as
                normal parameters preceded by '---' to delimit them from
                kernel parameters

                eg.  newisnt foocomp baa pincount=8 --- "String1" "String2" "String3"
                """
        if name == 'pincount':
            if value != None:
                numpins = int(value)
                have_numpins = True
                iplist.append(name)
                ipvlist.append(int(value))

        ## int instanceparams could be used an array size specifiers, so store them in iplist
        ## ignore uint, will be just for hex2dec conversions etc. not array sizing
        else :
            if (mptype == 'int'):
                iplist.append(name)
                if value == None: v = 0
                else:
                    if value.find("0x"): v = int(value, 16)
                    else : v = int(value)
                ipvlist.append(v)

###  Now set max and default pincount sizes  ###########################

    if have_numpins :  # if pincount being used
        if (not options.get("MAXCOUNT")) :
            option("MAXCOUNT", numpins)
        print >>f, "#define MAXCOUNT %d" % options.get("MAXCOUNT" , 1)
        option("DEFAULTCOUNT", numpins)
        print >>f, "#define DEFAULTCOUNT %d\n" % options.get("DEFAULTCOUNT" , 1)
        have_count = True

########################################################################
##  Helper functions

    def StrIsInt(s):
        try:
            int(s)
            return True
        except ValueError:
            return False

    def get_varval(array):
        m = 0
        q = 0
        global maxpins
        if (len(iplist)) :
            while (m < len(iplist)) :
                if iplist[m] == array :
                    q = ipvlist[m]
                    if have_maxpins and q > maxpins:
                        q = maxpins
                    return q
                else :
                    if StrIsInt(array) :
                        q = int(array)
                        if q > maxpins :
                            q = maxpins
                        return q
                m += 1

    def setmax(array) :
        m = 0
        q = 0
        global maxpins
        if array == None : return
         # if it is just an int
        if StrIsInt(array) :
            q = int(array)
            if q > maxpins :
                maxpins = q
        else:
            if (len(iplist)) :
                while (m < len(iplist)) :
                    # if initialiser is name string
                    if iplist[m] == array :
                        if ipvlist[m] > maxpins :
                            maxpins = ipvlist[m]
                    m += 1

##############################################################################
#  Set maxpins to highest value used as array size specifier
#  All arrays are set to this size, allowing any <= values in instances
#  preventing any array overruns
##############################################################################

#  if value of any array sizing param is higher than maxpins - reset maxpins
    for name, type, array, dir, value in pins:
        setmax(array)

    for name, type, array, dir, value in pin_ptrs:
        setmax(array)

########################################################################
    if maxpins :
        have_maxpins = True

#  if numpins and or maxpins specified
    if have_numpins and (not have_maxpins) :
        maxpins = numpins
        have_maxpins = True
    else:
        if numpins > maxpins :
            numpins = maxpins
    mp = int(options.get("MAXCOUNT" , 1) )
    if maxpins < mp: maxpins = mp

############################  RTAPI_IP / MP declarations ########################

    for name, mptype, doc, value in instanceparams:
        if (mptype == 'int'):
            if value == None: v = 0
            else: v = value
            print >>f, "static %s %s = %d;" % (mptype, to_c(name), int(v))
            print >>f, "RTAPI_IP_INT(%s, \"%s\");\n" % (to_c(name), doc)
        if (mptype == 'u32'):
            if value == None: v = 0
            else: v = value
            print >>f, "static %s %s = %d;" % (mptype, to_c(name), int(v))
            print >>f, "RTAPI_IP_UINT(%s, \"%s\");\n" % (to_c(name), doc)


################################################################################################
#  Still process these but don't advertise them - possible future application in base component

    for name, mptype, value in moduleparams:
        if (mptype == 'int'):
            if value == None: v = 0
            else: v = value
            print >>f, "static %s %s = %d;" % (mptype, to_c(name), int(v))
            print >>f, "RTAPI_MP_INT(%s, \"Module integer param '%s'\");\n" % (to_c(name), to_c(name))
        else:
            if value == None: strng = "\"\\0\"";
            else: strng = value
            print >>f, "static char *%s = %s;" % (to_c(name), strng)
            print >>f, "RTAPI_MP_STRING(%s, \"Module string param '%s'\");\n" % (to_c(name), to_c(name))

################ struct declaration ###############################################################



    print >>f, "struct inst_data\n    {"

    for name, type, array, dir, value in pins:
        if array:
            print >>f, "    hal_%s_t *%s[%s];" % (type, to_c(name), maxpins )
        else:
            print >>f, "    hal_%s_t *%s;" % (type, to_c(name))
        names[name] = 1

    for name, type, array, dir, value in pin_ptrs:
        if array:
            print >>f, "    %s_pin_ptr %s[%s];" % (type, to_c(name), maxpins )
        else:
            print >>f, "    %s_pin_ptr %s;" % (type, to_c(name))
        names[name] = 1

    for name, dir, value in rings:
        print >>f, "    ringbuffer_t *%s;" % (to_c(name))
        names[name] = 1

    for type, name, array, value in variables:
        if array:
            ## same assumptions to pins do not apply to variables, there may be no linkage between pin numbers and variables
            ## test whether a number or a text label, in latter case size array to maxpins and assume a linkage to pins
            ## because value is likely an instanceparam
            if StrIsInt(array) :
                q = int(array)
            else:
                q = maxpins
            print >>f, "    %s %s[%s];" % (type, name, q )
        else:
            print >>f, "    %s %s;" % (type, name)

    for type, name, array, value in userdef_types:
        if array:
            if StrIsInt(array) :
                q = int(array)
            else:
                q = maxpins
            print >>f, "    %s %s[%s];" % (type, name, q )
        else:
            print >>f, "    %s %s;" % (type, name)

    # if int instanceparam exists, echo its value in inst_data
    for name, mptype, doc, value in instanceparams:
        if ((mptype == 'int') or (mptype == "u32")) and (name != "pincount"):
            print >>f, "    int local_%s;" % to_c(name)

    print >>f, "    int local_argc;"
    print >>f, "    char **local_argv;"

    ##local copy used in function and set to default value
    if have_numpins:
        print >>f, "    int local_pincount;"
    print >>f, "    };"

############## extra headers and forward defines of functions  ##########################

    print >>f
    ## maxpins holds max array size required in advance of any knowledge of the pincount arg
    ## takes the highest value found in any of the arrays
    if have_maxpins:
        print >>f, "static int maxpins __attribute__((unused)) = %d;" % int(maxpins)
    else:
        print >>f, "static int maxpins __attribute__((unused)) = %d;" % int(numpins)

    for name, fp in functions:
        global funct_name
        global funct_
        if names.has_key(name):
            Error("Duplicate item name: %s" % name)
        if funct_ :
            print >>f, "static int %s(void *arg, const hal_funct_args_t *fa);\n" % funct_name
        else :
            print >>f, "static int %s(void *arg, const hal_funct_args_t *fa);\n" % to_c(name)
        names[name] = 1

    print >>f, "static int instantiate(const int argc, char* const *argv);\n"
    # we always have a delete function now - to free local_argv
    print >>f, "static int delete(const char *name, void *inst, const int inst_size);\n"
    if options.get("extra_inst_setup") :
        print >>f, "static int extra_inst_setup(\n" \
            "struct inst_data* ip, const char *name, int argc,\n" \
            "char* const *argv);\n"
    if options.get("extra_inst_cleanup"):
        print >>f, "static void extra_inst_cleanup(const char *name, void *inst, const int inst_size);\n"

    if not options.get("no_convenience_defines"):
    # capitalised defines removed to enforce lowercase C boolean values
        print >>f, "#undef TRUE"
        print >>f, "#undef FALSE"
        print >>f, "#undef true"
        print >>f, "#define true (1)"
        print >>f, "#undef false"
        print >>f, "#define false (0)"

    print >>f

###################################  funct()  #######################################
#    Stub for future use if required within the base component
#
#    print >>f, "static int funct(const void *arg, const hal_funct_args_t *fa)\n{"
#    print >>f, "long period __attribute__((unused)) = fa_period(fa);\n"
#
#    print >>f, "    // the following accessors are available here:"
#    print >>f, "    // fa_period(fa) - formerly 'long period'"
#    print >>f, "    // fa_thread_start_time(fa): _actual_ start time of thread invocation"
#    print >>f, "    // fa_start_time(fa): _actual_ start time of function invocation"
#    print >>f, "    // fa_thread_name(fa): name of the calling thread (char *)"
#    print >>f, "    // fa_funct_name(fa): name of the this called function (char *)"
#    print >>f, "    return 0;\n}\n"
#
#
#
###########################  export_halobjs()  ######################################################

    print >>f, "static int export_halobjs(\n" \
        "struct inst_data *ip, int owner_id, const char *name,\n" \
        "const int argc, char * const *argv)\n{"

    print >>f, "    char buf[HAL_NAME_LEN + 1];"
    print >>f, "    int r = 0;"
    print >>f, "    int j __attribute__((unused)) = 0;"
    print >>f, "    int z __attribute__((unused)) = 0;"

    for name, type, array, dir, value in pins:
        if array:
            print >>f, "    z = %s;" % array
            print >>f, "    if(z > maxpins)\n       z = maxpins;"
            print >>f, "    for(j=0; j < z; j++)\n        {"
            print >>f, "        r = hal_pin_%s_newf(%s, &(ip->%s[j]), owner_id," % (type, dirmap[dir], to_c(name))
            #print >>f, "        r = hal_pin_%s_newf(%s, &(ip->%s[j]), owner_id," % (type, dirmap[dir], to_hal(name))
            print >>f, "            \"%%s%s\", name, j);" % to_hal("." + name)
            print >>f, "        if(r != 0) return r;"
            if value is not None:
                print >>f, "            *(ip->%s[j]) = %s;" % (to_c(name), value)
            print >>f, "        }\n"
        else:
            print >>f, "    r = hal_pin_%s_newf(%s, &(ip->%s), owner_id," % (type, dirmap[dir], to_c(name))
            #print >>f, "    r = hal_pin_%s_newf(%s, &(ip->%s), owner_id," % (type, dirmap[dir], to_hal(name))
            print >>f, "            \"%%s%s\", name);" % to_hal("." + name)
            print >>f, "    if(r != 0) return r;\n"
            if value is not None:
                print >>f, "    *(ip->%s) = %s;" % (to_c(name), value)

    for name, type, array, dir, value in pin_ptrs:
        if array:
            print >>f, "    z = %s;" % array
            print >>f, "    if(z > maxpins) z = maxpins;"
            print >>f, "    for(j=0; j < z; j++)\n        {"
            print >>f, "        ip->%s[j] = halx_pin_%s_newf(%s, owner_id," % (to_c(name), type , dirmap[dir] )
            #print >>f, "        ip->%s[j] = halx_pin_%s_newf(%s, owner_id," % (to_hal(name), type , dirmap[dir] )
            print >>f, "            \"%%s%s\", name, j);" % to_hal("." + name)
            print >>f, "        if (%s_pin_null(ip->%s[j]))\n            return _halerrno;" % (type, to_c(name))
            if value is not None:
                print >>f, "        set_%s_pin(ip->%s[j], %s);" % ( type, to_c(name), value)
            print >>f, "        }\n"
        else:
            print >>f, "\n    ip->%s = halx_pin_%s_newf(%s, owner_id," % (to_c(name), type, dirmap[dir] )
            #print >>f, "\n    ip->%s = halx_pin_%s_newf(%s, owner_id," % (to_hal(name), type, dirmap[dir] )
            print >>f, "            \"%%s%s\", name);" % to_hal("." + name)
            print >>f, "    if (%s_pin_null(ip->%s))\n            return _halerrno;\n" % (type, to_c(name))
            if value is not None:
                print >>f, "    set_%s_pin(ip->%s, %s);\n" % (type, to_c(name), value)

    if rings :
        print >>f, "    unsigned flags;\n"
        for name, dir, value in rings:
            buf =  "    if((retval = hal_ring_attachf(&(ip->%s)," % to_c(name)
            buf += "&flags,  \"%s."
            buf += "%s\", name)) < 0)" % dir
            print >>f, buf
            print >>f, "        return retval;"
            print >>f, "    if ((flags & RINGTYPE_MASK) != RINGTYPE_RECORD) {"
            print >>f, "        HALERR(\"ring %s.in not a record mode ring: mode=%d\",name, flags & RINGTYPE_MASK);"
            print >>f, "        return -EINVAL;\n    }\n"

        print >>f, "    ip->to_rt_rb.header->reader = owner_id;"
        print >>f, "    ip->from_rt_rb.header->writer = owner_id;\n"

    for type, name, array, value in variables:
        if value is None: continue
        name = name.replace("*", "")
        if array:
            print >>f, "    z = %s;"  % array
            print >>f, "    for(j=0; j < z; j++)\n       {"
            print >>f, "        ip->%s[j] = %s;" % (name, value)
            print >>f, "        }\n"
        else:
            print >>f, "    ip->%s = %s;" % (name, value)

    for type, name, array, value in userdef_types:
        ptr = 0
        if value is None: continue
        if name[0] == '*': ptr = 1
        else : ptr = 0
        name = name.replace("*", "")
        if array:
            print >>f, "    z = %s;"  % array
            print >>f, "    for(j=0; j < z; j++)\n       {"
            if ptr:
                print >>f, "        *(ip->%s[j]) = %s;" % (name, value)
            else :
                print >>f, "        ip->%s[j] = %s;" % (name, value)
            print >>f, "        }\n"
        else:
            if ptr:
                print >>f, "    *(ip->%s) = %s;" % (name, value)
            else :
                print >>f, "    ip->%s = %s;" % (name, value)

    if have_count:
        print >>f, "\n// if not set by instantiate() set to default"
        print >>f, "    if(! ip->local_pincount || ip->local_pincount == -1)"
        print >>f, "         ip->local_pincount = DEFAULTCOUNT;\n"
        print >>f, "    hal_print_msg(RTAPI_MSG_DBG,\"export_halobjs() ip->local_pincount set to %d\", ip->local_pincount);\n"

    ## echo instanceparam int values into inst_data, except local_pincount, which is done explicitly
    for name, mptype, doc, value in instanceparams:
        if ((mptype == 'int') or (mptype == "u32")) and (name != "pincount"):
            print >>f, "    ip->local_%s = %s;" % (to_c(name), to_c(name))
    print >>f, "\n    ip->local_argv = halg_dupargv(1, argc, argv);\n"
    print >>f, "    ip->local_argc = argc;\n"

    for name, fp in functions:
        print >>f, "\n    // exporting an extended thread function:"
        print >>f, "    hal_export_xfunct_args_t %s_xf = " % to_c(name)
        print >>f, "        {"
        print >>f, "        .type = FS_XTHREADFUNC,"
        if funct_ :
            print >>f, "        .funct.x = %s," % funct_name
        else:
            print >>f, "        .funct.x = %s," % to_c(name)
        print >>f, "        .arg = ip,"
        print >>f, "        .uses_fp = %d," % int(fp)
        print >>f, "        .reentrant = 0,"
        print >>f, "        .owner_id = owner_id"
        print >>f, "        };\n"

        strng = "    rtapi_snprintf(buf, sizeof(buf),\"%s"
        if (len(functions) == 1) and name == "_":
            strng += ".funct\", name);"
        else :
            strng += ".%s\", name);" % (to_hal(name))
        print >>f, strng

        print >>f, "    r = hal_export_xfunctf(&%s_xf, buf, name);" % to_c(name)
        print >>f, "    if(r != 0)\n        return r;"

    print >>f, "    return 0;"
    print >>f, "}"



###########################  instantiate() ###############################################################

    print >>f, "\n// constructor - init all HAL pins, funct etc here"
    print >>f, "static int instantiate(const int argc, char* const *argv)\n{"
    print >>f, "struct inst_data *ip;"
    print >>f, "// argv[0]: component name"
    print >>f, "const char *name = argv[1];" # instance name
    print >>f, "int r;"
    if options.get("extra_inst_setup"):
        print >>f, "int k;"
    print >>f, "\n// allocate a named instance, and some HAL memory for the instance data"
    print >>f, "int inst_id = hal_inst_create(name, comp_id, sizeof(struct inst_data), (void **)&ip);\n"
    print >>f, "    if (inst_id < 0)\n        return -1;\n"

    print >>f, "// here ip is guaranteed to point to a blob of HAL memory of size sizeof(struct inst_data)."
    print >>f, "    hal_print_msg(RTAPI_MSG_DBG,\"%s inst=%s argc=%d\",__FUNCTION__, name, argc);\n"
    print >>f, "// Debug print of params and values"


    for name, mptype, doc, value in instanceparams:
        if (mptype == 'int'):
            strg = "    hal_print_msg(RTAPI_MSG_DBG,\"%s: int instance param: %s=%d\",__FUNCTION__,"
            strg += "\"%s\", %s);" % (to_c(name), to_c(name))
            print >>f, strg

    if have_count:
        for name, mptype, doc, value in instanceparams:
            if name == 'pincount':
                if value != None:
                    print >>f, "//  if pincount=NN is passed, set local variable here, if not set to default"
                    print >>f, "    int pin_param_value = pincount;"
                    print >>f, "    if((pin_param_value == -1) || (pin_param_value == 0))"
                    print >>f, "        pin_param_value = DEFAULTCOUNT;"
                    print >>f, "    else if((pin_param_value > 0) && (pin_param_value > MAXCOUNT))"
                    print >>f, "        pin_param_value = MAXCOUNT;"
                    print >>f, "    ip->local_pincount = pincount = pin_param_value;"
                    print >>f, "    hal_print_msg(RTAPI_MSG_DBG,\"ip->local_pincount set to %d\", pin_param_value);"


    print >>f, "\n// These pins - pin_ptrs- functs will be owned by the instance, and can be separately exited with delinst"

    print >>f, "    r = export_halobjs(ip, inst_id, name, argc, argv);"

    if options.get("extra_inst_setup"):
        print >>f, "    // if the extra_inst_setup returns non zero will abort module creation"
        print >>f, "    k = extra_inst_setup(ip, name, argc, argv);"
        print >>f, "    if(k != 0)"
        print >>f, "        return k;\n"

    #print >>f, "    if(r == 0)"
    #print >>f, "        hal_print_msg(RTAPI_MSG_DBG,\"%s - instance %s creation SUCCESSFUL\",__FUNCTION__, name);"
    #print >>f, "    else"
    #print >>f, "        hal_print_msg(RTAPI_MSG_DBG,\"%s - instance %s creation ABORTED\",__FUNCTION__, name);"
    if have_count:
        print >>f, "//reset pincount to -1 so that instantiation without it will result in DEFAULTCOUNT"
        print >>f, "    pincount = -1;\n"

    print >>f, "    return r;\n}"

##############################  rtapi_app_main  ######################################################

    print >>f, "\nint rtapi_app_main(void)\n{"
    print >>f, "    comp_id = hal_xinit(TYPE_RT, 0, 0, instantiate, delete, compname);\n"
    print >>f, "    if (comp_id < 0)\n"
    print >>f, "        return -1;\n"

################  stub to allow 'base component to have function if req later ####
#
#    print >>f, "    // exporting an extended thread function:"
#    print >>f, "    hal_export_xfunct_args_t xtf = "
#    print >>f, "        {"
#    print >>f, "        .type = FS_XTHREADFUNC,"
#    print >>f, "        .funct.x = (void *) funct,"
#    print >>f, "        .arg = \"x-instance-data\","
#    print >>f, "        .uses_fp = 0,"
#    print >>f, "        .reentrant = 0,"
#    print >>f, "        .owner_id = comp_id"
#    print >>f, "        };\n"
#
#    print >>f, "    if (hal_export_xfunctf(&xtf,\"%s.rtfunct\", compname))"
#    print >>f, "        return -1;"
##################################################################################

    print >>f, "    hal_ready(comp_id);\n"

    print >>f, "    return 0;\n}\n"

###############################  rtapi_app_exit()  #####################################################

    print >>f, "void rtapi_app_exit(void)\n{"

    print >>f, "    hal_exit(comp_id);"
    print >>f, "}\n"

#########################   delete()  ####################################################################
    print >>f, "// Custom destructor - delete()"
    print >>f, "// pins, pin_ptrs, and functs are automatically deallocated regardless if a"
    print >>f, "// destructor is used or not (see below)"
    print >>f, "//"
    print >>f, "// Some objects like vtables, rings, threads are not owned by a component"
    print >>f, "// interaction with such objects may require a custom destructor for"
    print >>f, "// cleanup actions"
    print >>f, "// Also allocated memory that hal_lib will know nothing about ie local_argv"
    print >>f, "//"
    print >>f, "// NB: if a customer destructor is used, it is called"
    print >>f, "// - after the instance's functs have been removed from their respective threads"
    print >>f, "//   (so a thread funct call cannot interact with the destructor any more)"
    print >>f, "// - any pins and params of this instance are still intact when the destructor is"
    print >>f, "//   called, and they are automatically destroyed by the HAL library once the"
    print >>f, "//   destructor returns\n\n"
    print >>f, "static int delete(const char *name, void *inst, const int inst_size)\n{\n"

#################  how to print contents of params if required #####################################################
#
#    print >>f, "    hal_print_msg(RTAPI_MSG_DBG,\"%s inst=%s size=%d %p\\n\", __FUNCTION__, name, inst_size, inst);"
#    print >>f, "// Debug print of params and values"
#    for name, mptype, doc, value in instanceparams:
#        if (mptype == 'int'):
#            strg = "    hal_print_msg(RTAPI_MSG_DBG,\"%s: int instance param: %s=%d\",__FUNCTION__,"
#            strg += "\"%s\", %s);" % (to_c(name), to_c(name))
#            print >>f, strg
#        else:
#            strg = "    hal_print_msg(RTAPI_MSG_DBG,\"%s: string instance param: %s=%s\",__FUNCTION__,"
#            strg += "\"%s\", %s);" % (to_c(name), to_c(name))
#            print >>f, strg
#####################################################################################################################

    if options.get("extra_inst_cleanup"):
        print >>f, "    return extra_inst_cleanup(name, inst, inst_size);"
    else:
	print >>f, "    struct inst_data *ip = inst;\n"
	print >>f, "    HALDBG(\"Entering delete() : inst=%s size=%d %p local_argv = %p\\n\", name, inst_size, inst, ip->local_argv);\n"
	print >>f, "    HALDBG(\"Before free ip->local_argv[0] = %s\\n\", ip->local_argv[0]);\n"
        print >>f, "   	halg_free_argv(1, ip->local_argv);\n"
        print >>f, "    HALDBG(\"Now ip->local_argv[0] = %s\\n\", ip->local_argv[0]);\n"
    print >>f, "    return 0;\n"
    print >>f, "}\n"

######################  preliminary defines before user FUNCTION(_) ######################################

    print >>f
    if not options.get("no_convenience_defines"):
        print >>f, "#undef FUNCTION"
        if funct_ :
            print >>f, "#define FUNCTION(name) static int %s(void *arg, const hal_funct_args_t *fa)" % funct_name
        else :
            print >>f, "#define FUNCTION(name) static int name(void *arg, const hal_funct_args_t *fa)"
        if options.get("extra_inst_setup"):
            print >>f, "// if the extra_inst_setup returns non zero it will abort the module creation"
            print >>f, "#undef EXTRA_INST_SETUP"
            print >>f, "#define EXTRA_INST_SETUP() \\\n" \
                "static int extra_inst_setup( \\\n" \
                "struct inst_data *ip, const char *name, \\\n" \
                "int argc, char* const *argv)"
        if options.get("extra_inst_cleanup"):
            print >>f, "#undef EXTRA_INST_CLEANUP"
            print >>f, "#define EXTRA_INST_CLEANUP() static void extra_inst_cleanup(const char *name, void *inst, const int inst_size)"
        print >>f, "#undef fperiod"
        print >>f, "#define fperiod (period * 1e-9)"

        for name, type, array, dir, value in pins:
            print >>f, "#undef %s" % to_c(name)
            if array:
                if dir == 'in':
                    print >>f, "#define %s(i) (0+*(ip->%s[i]))" % (to_c(name), to_c(name))
                else:
                    print >>f, "#define %s(i) (*(ip->%s[i]))" % (to_c(name), to_c(name))
            else:
                if dir == 'in':
                    print >>f, "#define %s (0+*ip->%s)" % (to_c(name), to_c(name))
                else:
                    print >>f, "#define %s (*ip->%s)" % (to_c(name), to_c(name))

        for name, type, array, dir, value in pin_ptrs:
            print >>f, "#undef %s" % to_c(name)
            if array:
                print >>f, "#define %s(i) (ip->%s[i])" % (to_c(name), to_c(name))
            else:
                print >>f, "#define %s (ip->%s)" % (to_c(name), to_c(name))

        for type, name, array, value in variables:
            name = name.replace("*", "")
            print >>f, "#undef %s" % name
            if array:
                print >>f, "#define %s(i) (ip->%s[i])" % (to_c(name), to_c(name))
            else:
                print >>f, "#define %s (ip->%s)" % (to_c(name), to_c(name) )

        for type, name, array, value in userdef_types:
            ptr = 0
            if name[0] == '*': ptr = 1
            else : ptr = 0
            name = name.replace("*", "")
            print >>f, "#undef %s" % name
            if array:
                if ptr :
                    print >>f, "#define %s(i) (*(ip->%s[i]))" % (to_c(name), to_c(name))
                else :
                    print >>f, "#define %s(i) (ip->%s[i])" % (to_c(name), to_c(name))
            else:
                if ptr :
                    print >>f, "#define %s (*(ip->%s))" % (to_c(name), to_c(name) )
                else :
                    print >>f, "#define %s (ip->%s)" % (to_c(name), to_c(name) )

        for name, mptype, doc, value in instanceparams:
            if ((mptype == 'int') or (mptype == "u32")) and (name != "pincount"):
                print >>f, "#undef local_%s" % to_c(name)
                print >>f, "#define local_%s (ip->local_%s)" % (to_c(name), to_c(name))
        if have_numpins:
            print >>f, "#undef local_pincount"
            print >>f, "#define local_pincount (ip->local_pincount)"

        print >>f, "#undef local_argc"
        print >>f, "#define local_argc (ip->local_argc)"

        print >>f, "#undef local_argv"
        print >>f, "#define local_argv(i) (ip->local_argv[i])"

        if (pin_ptrs):
            print >>f, "#include <hal_accessor_macros.h>"
    print >>f
    print >>f

#########################  Epilogue - FUNCTION(_) printed in direct from file ################

def epilogue(f):
    print >>f

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


    ######################  docs man pages etc  ###########################################

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
    return s


def to_hal_man(s):
    s = "%s.%s" % (comp_name, s)
    s = s.replace("_", "-")
    s = s.rstrip("-")
    s = s.rstrip(".")
    s = re.sub("#+", lambda m: "\\fI" + "M" * len(m.group(0)) + "\\fB", s)
    return s

##############################################################################
# asciidoc
############

def adocument(filename, outfilename, frontmatter):
    if outfilename is None:
        outfilename = os.path.splitext(filename)[0] + ".asciidoc"

    a, b = parse(filename)
    f = open(outfilename, "w")

    if frontmatter:
        print >>f, "---"
        for fm in frontmatter:
            print >>f, fm
        print >>f, "edit-path: src/%s" % (filename)
        print >>f, "generator: instcomp"
	print >>f, "description: This page was generated from src/%s. Do not edit directly, edit the source." % filename
        print >>f, "---"
        print >>f, ":skip-front-matter:\n"

    print >>f, "= Machinekit Documentation"

    print >>f, ""
    print >>f, "== HAL Component -- %s" % (comp_name.upper())
    print >>f, ""
    print >>f, "=== INSTANTIABLE COMPONENTS -- General"
    print >>f, ""
    print >>f, "All instantiable components can be loaded in two manners"
    print >>f, ""
    print >>f, "[%hardbreaks]"    
    print >>f, "Using *loadrt* with or without _count=_ | _names=_ parameters as per legacy components"
    print >>f, "Using *newinst*, which names the instance and allows further parameters and arguments"
    print >>f, "primarily _pincount=_ which can set the number of pins created for that instance (where applicable)"
    print >>f, ""

    print >>f, "=== NAME"
    print >>f, ""
    doc = finddoc('component')
    if doc and doc[2]:
        if '\n' in doc[2]:
            firstline, rest = doc[2].split('\n', 1)
        else:
            firstline = doc[2]
            rest = ''
        print >>f, "==== %s -- %s" % (doc[1], firstline)
    else:
        rest = ''
        print >>f, "==== %s" % doc[1]
    print >>f, ""
    
    print >>f, "=== SYNOPSIS"
    print >>f, ""
    if rest:
        print >>f, "*%s*" % rest
    else:
        rest = ''
        print >>f, "*%s*" % doc[1]
    print >>f, ""    
    
    print >>f, "=== USAGE SYNOPSIS"
    print >>f, ""
    if rest:
        print >>f, "*%s*" % rest
        print >>f, ""                    
    else:
        print >>f, "[%hardbreaks]"    
        print >>f, "*loadrt %s*" % comp_name
        print >>f, "OR"
        print >>f, "*newinst %s <newinstname>* [ *pincount*=_N_ | *iprefix*=_prefix_ ] [*instanceparamX*=_X_ | *argX*=_X_ ]" % comp_name
        print >>f, ""       
        print >>f, "( args in [ ] denote possible args and parameters, may not be used in all components )"
        for type, name, default, doc in modparams:
            print >>f, "[%s=_N_]" % name,
        print >>f

        hasparamdoc = False
        for type, name, default, doc in modparams:
            if doc: hasparamdoc = True

        if hasparamdoc:
            print >>f, ""
            print >>f, "* modparams:"
            for type, name, default, doc in modparams:
                print >>f, ""
                print >>f, "** *%s*" % name,
                if default:
                    print >>f, "[default: %s]" % default
                else:
                    print >>f
                print >>f, doc
            print >>f, ""

    doc = finddoc('descr')
    if doc and doc[1]:
        print >>f, "=== DESCRIPTION"
        print >>f, ""
        print >>f, "%s" % doc[1]
        print >>f, ""            

    doc = finddoc('sf_doc')
    if doc and doc[1]:
        print >>f, "=== EXTRA INFO"
        print >>f, ""
        print >>f, "%s" % doc[1]
        print >>f, ""

    if functions:
        print >>f, "=== FUNCTIONS"
        print >>f, ""
        for _, name, fp, doc in finddocs('funct'):
            if name != None and name != "_":
                print >>f, "*%s.N.%s.funct*" % (comp_name, to_hal(name)) ,
            else :
                print >>f, "*%s.N.funct*" % comp_name ,
            print >>f, "\n( OR"
            if name != None and name != "_":
                print >>f, "*<newinstname>.%s.funct*"  % to_hal(name) ,
            else :
                print >>f, "*<newinstname>.funct*" ,
            if fp:
                print >>f, "(requires a floating-point thread) )"
            else:
                print >>f, " )"
            print >>f, ""
            if doc:
        	print >>f, doc
            print >>f, ""    

    print >>f, "=== PINS"
    print >>f, ""    
    for _, name, type, array, dir, doc, value in finddocs('pin'):
        print >>f, ""
        print >>f, "*%s.N.%s*" % (comp_name, to_hal(name)),
        print >>f, type, dir,
        if array:
            sz = name.count("#")
            print >>f, " (%s=%s..%s)" % ("M" * sz, "0" * sz , array),
        if value:
            print >>f, "(default: _%s_)" % value

	print >>f, "( OR"

        print >>f, "*<newinstname>.%s*" % to_hal(name),
        print >>f, type, dir,
        if array:
            sz = name.count("#")
            print >>f, " (%s=%s..%s)" % ("M" * sz, "0" * sz , array),
        if value:
            print >>f, "(default: _%s_) )" % value
        else:
            print >>f, ")"
        if doc:
    	    print >>f, " - %s\n" % doc
        print >>f, ""    

    if instanceparams:
        print >>f, "=== INST_PARAMETERS"
        print >>f, ""
        for _, name, type, doc, value in finddocs('instanceparam'):
            print >>f, "*%s*" % name,
            print >>f, type,
            if value:
                print >>f, "(default: _%s_)" % value
            if doc:
    		print >>f, " - %s\n" % doc
        print >>f, ""
        
    if moduleparams:
        print >>f, "=== MODULE_PARAMETERS"
        print >>f, ""
        for _, name, type, doc, value in finddocs('moduleparam'):
            print >>f, "*%s*" % name,
            print >>f, type,
            if value:
                print >>f, "(default: _%s_)" % value
            if doc:
                print >>f, " - %s\n" % doc
        print >>f, ""    
    
    doc = finddoc('see_also')
    if doc and doc[1]:
        print >>f, "=== SEE ALSO"
        print >>f, ""
        print >>f, "%s" % doc[1]
        print >>f, ""    
    
    doc = finddoc('notes')
    if doc and doc[1]:
        print >>f, "=== NOTES"
        print >>f, ""
        print >>f, "%s" % doc[1]
        print >>f, ""    

    doc = finddoc('author')
    if doc and doc[1]:
        print >>f, "=== AUTHOR"
        print >>f, ""
        print >>f, "%s" % doc[1]
        print >>f, ""    
        
    doc = finddoc('license')
    if doc and doc[1]:
        print >>f, "=== LICENCE"
        print >>f, ""
        print >>f, "%s" % doc[1]
        print >>f, ""    



###########################################################


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

        if (not pins) and (not pin_ptrs):
            raise SystemExit, "Error: Component must have at least one pin or pin_ptr"
        if not "return" in b:
            raise SystemExit, """ \
            Error: Function code must return with an integer value.
           The default is 0, other values are for future use to
           describe error states and improve debugging
           """
        prologue(f)
        lineno = a.count("\n") + 3

################   parse the remainder of the file to add the function prelims  #####

        have_func = False
        insert = False
        if "FUNCTION" in b:
            c = ""
            q = ""
            f.write("#line %d \"%s\"\n" % (lineno, filename))
            linelist = b.split("\n")
            z = len(linelist)
            y = 0
            while(y < z) :
                q = linelist[y]
                if have_func :
                    if "{" in q:
                        have_func = False
                        insert = True
                else :
                    if "FUNCTION" in q and not "{" in q:
                        have_func = True
                    else :
                        if "FUNCTION" in q and "{" in q:
                            insert = True
                if insert :
                    g,h = q.split("{")
                    c += g
                    c += "{"
                    c += "\nlong period __attribute__((unused)) = fa_period(fa);\n"
                    c += "struct inst_data *ip __attribute__((unused)) = arg;\n\n"
                    c += h
                    insert = False
                else :
                    c += q
                    c += "\n"
                y = y + 1

            f.write(c)

        # if the code is loose because there is just one function
        elif len(functions) == 1:
            f.write("FUNCTION(%s)\n{\n" % functions[0][0])
            f.write("long period __attribute__((unused)) = fa_period(fa);\n")
            f.write("struct inst_data *ip __attribute__((unused)) = arg;\n\n")
            f.write("#line %d \"%s\"\n" % (lineno, filename))
            f.write(b)
            f.write("\n}\n")
        else:
            raise SystemExit, "Error: Must use FUNCTION() when more than one function is defined"
        epilogue(f)
        f.close()

        if mode != PREPROCESS:
            build_rt(tempdir, outfilename, mode, filename)

    finally:
        shutil.rmtree(tempdir)

def usage(exitval=0):
    print """%(name)s: Build, compile, and install Machinekit HAL components

Usage:
           %(name)s [ --compile (-c) | --preprocess (-p) | --document (-d) | --view-doc (-v) ] compfile...
    [sudo] %(name)s [ --install (-i) |--install-doc (-j) ] compfile...
           %(name)s --print-modinc
""" % {'name': os.path.basename(sys.argv[0])}
    raise SystemExit, exitval


#####################################  main  ##################################

def main():
    global require_license
    require_license = True
    mode = PREPROCESS
    outfile = None
    frontmatter = []
    userspace = False
    try:
        opts, args = getopt.getopt(sys.argv[1:], "f:lijcpdo:h?",
                           ['frontmatter=', 'install', 'compile', 'preprocess', 'outfile=',
                            'document', 'help', 'userspace', 'install-doc',
                            'view-doc', 'require-license', 'print-modinc'])
    except getopt.GetoptError:
        usage(1)
    
    for k, v in opts:
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
        if k in ("-v", "--view-doc"):
            mode = VIEWDOC
        if k in ("--print-modinc",):
            mode = MODINC
        if k in ("-l", "--require-license"):
            require_license = True
        if k in ("-o", "--outfile"):
            if len(args) != 1:
                raise SystemExit, "Error: Cannot specify -o with multiple input files"
            outfile = v
        if k in ("-f", "--frontmatter"):
            frontmatter.append(v)
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
            if f.endswith(".icomp") and mode == DOCUMENT:
    		adocument(f, outfile, frontmatter)
            elif f.endswith(".icomp") and mode == VIEWDOC:
                tempdir = tempfile.mkdtemp()
                try:
                    outfile = os.path.join(tempdir, basename + ".asciidoc")
                    adocument(f, outfile, frontmatter)
                    cmd = "mank -f %s -p %s -s" % (basename, tempdir)
        	    os.system(cmd)
                finally:
        	    shutil.rmtree(tempdir)
            elif f.endswith(".icomp") and mode == INSTALLDOC:
    		manpath = os.path.join(BASE, "share/man/man9")
                if not os.path.isdir(manpath):
            	    manpath = os.path.join(BASE, "man/man9")
            	    outfile = os.path.join(manpath, basename + ".asciidoc")
        	print "INSTALLDOC", outfile
    		adocument(f, outfile, frontmatter)
            elif f.endswith(".icomp"):
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
