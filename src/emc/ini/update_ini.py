#!/usr/bin/env python3

THIS_VERSION = "1.1"

import sys
import os
import shutil
import linuxcnc
import re
import datetime

try:
    import tkinter
    from tkinter import messagebox
except ImportError:
    # tkinter is only needed for the -d dialogs. The --fix-identifiers mode
    # and the -f conversion must also work on systems without it (e.g.
    # headless builds).
    tkinter = None

def copysection(block):
    #Just makes a straight copy of blocks that don't need any work
    regex = r"^\s*\[%s\](.+?)(?:^\s*\[|\Z)" % block
    section = re.search(regex, inistring, re.M | re.DOTALL)
    newini.write("\n[%s]" % block)
    if section != None:
        newini.write(section.group(1))
        all_sections.remove(block)
    else:
         newini.write("\n#No Content\n")

def writeifexists(file, section, src_item, dest_item = "None"):
    #Writes a new entry to the file, but only if it exists
    if dest_item == 'None': dest_item = src_item
    val = ini.find(section, src_item)
    if val: file.write("%s = %s\n" % (dest_item, val))

# The ini parser restricts section and tag identifiers to letters, digits
# and underscore, not starting with a digit. Older configs (notably xhc-hb04
# pendant setups) used dashes in identifiers, which the historic parser
# accepted. The following text pass rewrites such identifiers with
# underscores so the file can be parsed again. It runs before any
# version-based conversion and regardless of the [EMC]VERSION content.
# A .bak copy is kept of every modified file and every rename is listed.

_ident_sec_re = re.compile(r"^(\s*)\[([^\]]+)\](.*)$")
_ident_tag_re = re.compile(r"^(\s*)([A-Za-z_][A-Za-z0-9_-]*)(\s*=.*)$")
_ident_ok_re = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")

def _ident_fix_name(name):
    # Returns the name with dashes replaced by underscores if the result is
    # a valid identifier and the original contained a dash, else None.
    if '-' not in name:
        return None
    fixed = name.replace('-', '_')
    if _ident_ok_re.match(fixed):
        return fixed
    return None

def fix_dashed_identifiers(ini_filename):
    # Rewrites dashed section and tag identifiers to underscores in
    # ini_filename, its #INCLUDE'd files and a custom xhc-hb04 layout
    # cfg found via [XHC_HB04_CONFIG]layout. Returns a list of
    # (path, old, new) renames.
    #
    # Every rewritten file gets a fresh .bak backup of its original
    # content. All files are processed in memory first and nothing is
    # written if any of the .bak files already exists, since an existing
    # .bak may be ancient or unrelated and overwriting it would lose the
    # true original. In that case the whole conversion fails.
    renames = []
    contents = {} # path => original text, doubles as the processed set
    outputs = {}  # path => rewritten text

    def process(path):
        path = os.path.normpath(path)
        if path in contents:
            return
        try:
            with open(path, 'r') as f:
                text = f.read()
        except (IOError, OSError):
            return # unreadable files are reported by the parser later
        contents[path] = text

        # Split into physical lines, keeping line endings for the rewrite
        phys = []
        endings = []
        for line in text.splitlines(keepends=True):
            body = line.rstrip('\r\n')
            phys.append(list(body))
            endings.append(line[len(body):])

        changed = False
        i = 0
        while i < len(phys):
            # Assemble one logical line, merging continuations like the
            # parser does: a line whose right-trimmed content ends in a
            # backslash continues on the next physical line. Only the
            # identifier of a section or tag is ever modified (a dash is
            # replaced by an underscore at the same character position),
            # so values, continuations and layout are preserved verbatim.
            logical = ""
            spans = [] # (physical line index, logical start, length) per part
            j = i
            while True:
                contrib = ''.join(phys[j]).rstrip()
                if contrib.endswith('\\'):
                    spans.append((j, len(logical), len(contrib) - 1))
                    logical += contrib[:-1]
                    j += 1
                    if j >= len(phys):
                        break # continuation on the last line: a parser error, but not ours to fix
                else:
                    spans.append((j, len(logical), len(contrib)))
                    logical += contrib
                    j += 1
                    break
            i = j

            def flip_dash(p):
                # Replace the dash at logical position p with an underscore
                # at the corresponding physical position.
                for phys_idx, log_start, length in spans:
                    if log_start <= p < log_start + length:
                        phys[phys_idx][p - log_start] = '_'
                        return

            def fix_span(start, end, old, new):
                nonlocal changed
                renames.append((path, old, new))
                for p in range(start, end):
                    if logical[p] == '-':
                        flip_dash(p)
                changed = True

            if logical.startswith("#INCLUDE") and len(logical) > 8 and logical[8] in ' \t':
                inc = os.path.expanduser(logical[8:].strip())
                if not os.path.isabs(inc):
                    inc = os.path.join(os.path.dirname(path), inc)
                process(inc)
                continue
            stripped = logical.lstrip()
            if stripped == '' or stripped[0] in '#;':
                continue
            m = _ident_sec_re.match(logical)
            if m:
                raw = m.group(2)
                ident = raw.strip()
                fixed = _ident_fix_name(ident)
                if fixed:
                    start = m.start(2) + len(raw) - len(raw.lstrip())
                    fix_span(start, start + len(ident), "[%s]" % ident, "[%s]" % fixed)
            else:
                m = _ident_tag_re.match(logical)
                if m:
                    fixed = _ident_fix_name(m.group(2))
                    if fixed:
                        fix_span(m.start(2), m.end(2), m.group(2), fixed)

        if changed:
            outputs[path] = ''.join(''.join(body) + ending
                                    for body, ending in zip(phys, endings))

    process(ini_filename)

    # A custom xhc-hb04 layout cfg is parsed by the same rules. Find it
    # with a loose text scan since the ini file may not parse yet.
    cfgs = []
    for path, text in contents.items():
        m = re.search(r"(?ms)^\s*\[XHC_HB04_CONFIG\](.*?)(?:^\s*\[|\Z)", text)
        if not m:
            continue
        lm = re.search(r"(?m)^\s*layout\s*=\s*([^;#\s]+)", m.group(1))
        if not lm:
            continue
        layout = lm.group(1).strip('{}')
        if layout in ("1", "2") or layout.startswith("LIB:"):
            continue # distribution-provided layout files
        cfg = os.path.expanduser(layout)
        if not os.path.isabs(cfg):
            cfg = os.path.join(os.path.dirname(path), cfg)
        if os.path.isfile(cfg):
            cfgs.append(cfg)
    for cfg in cfgs:
        process(cfg)

    # Refuse to overwrite an existing backup. Fail the whole conversion
    # before anything is written, so the include cascade stays atomic.
    conflicts = [path for path in outputs if os.path.exists(path + ".bak")]
    if conflicts:
        for path in conflicts:
            print("update_ini: %s.bak already exists, will not overwrite it" % path)
        print("update_ini: no files were changed, move or remove the existing .bak file(s) and retry")
        exit(1)

    # Back up and write file by file. If anything fails midway, restore
    # the files already processed from their fresh .bak copies and remove
    # those copies again, so the config is never left half-converted and a
    # retry is not blocked by leftover .bak files.
    written = []
    try:
        for path, text in outputs.items():
            shutil.copy(path, path + ".bak")
            written.append(path)
            with open(path, 'w') as f:
                f.write(text)
    except OSError as e:
        print("update_ini: failed to update %s: %s" % (path, e))
        for path in written:
            try:
                shutil.copy(path + ".bak", path)
                os.remove(path + ".bak")
            except OSError as e2:
                print("update_ini: could not restore %s: %s" % (path, e2))
                print("update_ini: %s may be inconsistent, its original content is in %s.bak" % (path, path))
        print("update_ini: no files were changed")
        exit(1)

    if outputs:
        # Verify that the converted cascade parses. The linuxcnc script
        # retries the parse itself, but standalone --fix-identifiers use
        # should not report success on a file that still fails, e.g. on
        # identifiers that can not be mapped to a valid one.
        try:
            linuxcnc.ini(ini_filename)
        except:
            print("update_ini: %s still fails to parse after the identifier conversion" % ini_filename)
            print("update_ini: the converted files are left in place, the originals are in the .bak files")
            exit(1)

    return renames

def report_identifier_fixes(renames):
    for path, old, new in renames:
        print("%s: %s => %s" % (path, old, new))
    if renames:
        print("update_ini: renamed %d dashed identifier(s) to use underscores (originals saved as .bak)" % len(renames))

force = 0
dialogs = 0
ident_only = 0
subs = {}
subs2 = {}

filename = None
for opt in sys.argv[1:]:
    if opt == '-d':
        if tkinter is None:
            print("update_ini: the -d option needs tkinter, which is not available")
            exit(1)
        dialogs = 1
        r = tkinter.Tk()
        r.option_add('*Dialog.msg.font', 'Times 12')
        r.option_add('*Dialog.msg.wrapLength', '6i')

    elif opt == '-f':
        force = 1
    elif opt == '--fix-identifiers':
        ident_only = 1
    elif opt[0] == '-':
        print ("Unknown command line option to update_ini, exiting")
        exit()
    elif os.path.isfile(opt):
        filename = opt

if filename == None:
    t = """Usage: update_ini [-d] [-f] [--fix-identifiers] filename.ini\n
If the -d flag is used then a dialog box will be displayed
describing the purpose of this script, and giving the user the option
to change their minds\nIf the -f flag is used then no questions will be
asked and the conversion will proceed blindly\nIf --fix-identifiers is
used then only dashed identifiers are rewritten to use underscores and
no version-based conversion is done"""
    if dialogs:
        messagebox.showerror('invalid options', str(t))
    elif not force:
        print(t)
    exit()

if dialogs:
    ret = messagebox._show("Confirm automatic update",
                           "This version of LinuxCNC separates the concepts of Axes and "
                           "Joints which necessitates changes to the INI and HAL files. "
                           "The changes required are described here:\n"
                           "http://linuxcnc.org/docs/2.9/html/ in the section "
                           "'Getting Started with LinuxCNC' -> 'Updating LinuxCNC'\n"
                           "The [EMC]VERSION data in your INI file indicates that your "
                           "configuration requires update.\n"
                           "A script exists that can attempt to automatically "
                           "reconfigure your configuration files.\nPress 'Yes' to perform "
                           "the conversion, 'No' to continue with the current configuration "
                           "files or 'Cancel' to exit LinuxCNC.\n"
                           "The process can not be automatically reversed, though a "
                           "backup version of your entire existing config will be created.",
                           messagebox.QUESTION, messagebox.YESNOCANCEL)
    if ret == 'cancel': exit(42)
    elif ret == 'no': exit(0)

# We want to work with the base INI file here, not the expanded version if #include is used
filename = re.sub(r'\.expanded', '', filename)

# Rewrite dashed identifiers to underscores before anything else, since a
# file containing them can not be parsed at all. This is independent of the
# [EMC]VERSION-based conversion below.
# FIXME: the versioned conversion backs up differently (a .old copy of the
# whole config). A config that needs both migrations ends up with .bak
# files next to a .old directory. Revisit how the two backup schemes fit
# together when the version 1.2 conversion lands (#3704).
report_identifier_fixes(fix_dashed_identifiers(filename))

if ident_only:
    exit(0)

try:
    ini = linuxcnc.ini(filename)
except:
    t =  "%s is not a valid INI file" % filename
    if dialogs:
        messagebox.showerror('invalid options', t)
    elif not force:
        print(t)
    exit()

version = ini.find('EMC', 'VERSION')
if not version:
    version = "0.0"

if version == "$Revision$":
    pass
elif version >= THIS_VERSION:
    t =  """The supplied INI file is already at version %s and should not need
    updating""" % version
    if dialogs:
        messagebox.showerror('conversion not needed', t)
    elif not force:
        print(t)
    exit()

if ini.find('KINS', 'JOINTS') and not force and not version == "1.0":
    if dialogs:
        if messagebox.askquestion("Already Converted",
        "The supplied INI file already has a [KINS] section. this probably "
        "means that it was previously converted by hand. Continue conversion?"
        "(Change [EMC]VERSION to %s to suppress these messages) "
        % THIS_VERSION) != 'yes':
            exit(0)
    else:
        if input("The supplied INI file already has a [KINS] section."
        "this probably means that it was previously converted by hand. "
        "Continue y/N? (Change [EMC]VERSION to %s to suppress these messages) "
        % THIS_VERSION) != "y":
            exit(0)

# Looks like we are good to go, so first let's put the original configs
# somewhere safe.

basedir = os.path.dirname(os.path.abspath(filename))
backupdir = os.path.join(basedir, os.path.splitext(os.path.basename(filename))[0] + ".old")

while os.path.isdir(backupdir):
    backupdir += ".old"
os.mkdir(backupdir)
old_ini =  os.path.join(backupdir, os.path.basename(filename))
for f in os.listdir(basedir):
    if os.path.isdir(os.path.join(basedir, os.path.basename(f))):
        pass
    else:
        shutil.copy(os.path.join(basedir, os.path.basename(f)), 
                    os.path.join(backupdir, os.path.basename(f)))

#From now on, we use the backup copy as the reference
ini = linuxcnc.ini(old_ini)

#And the hal files too.
halfiles = ini.findall('HAL', 'HALFILE')
halfiles += ini.findall('HAL', 'POSTGUI_HALFILE')
halfiles += ['touchy.hal']
print("halfiles = ", halfiles)

halpaths = []
for halfile in halfiles:
    if os.path.isfile(os.path.join(basedir, halfile)):
        halpaths.append(os.path.join(basedir, halfile))
    elif (os.path.isfile(os.path.join(basedir, "hallib", halfile))):
        os.system('cp ' + os.path.join(basedir, "hallib", halfile) + ' ' +
                   os.path.join(basedir, halfile))
        halpaths.append(os.path.join(basedir, halfile))
    elif halfile == "touchy.hal":
        pass
    else:
        print("halfile %s not found\n" % halfile)

print("halpaths = ", halpaths)

if version == "1.0":
    #Just update the version in the INI
    inistring = open(filename,'r').read()
    newini = open(filename, 'w')
    inistring = re.sub("VERSION *= *(.*)", "VERSION = %s" % THIS_VERSION, inistring)
    newini.write(inistring)
    newini.close()

if version == "$Revision$" or version < "1.0":
    
    inistring = open(filename,'r').read()
    newini = open(filename, 'w')
    # Get a list of all sections
    all_sections = re.findall(r"^\s*\[(.+)\]", inistring, re.MULTILINE)

    # A C-style Switch would be nice here, to allow us to fall through successive
    # version updates.
    # At the moment there is only one update, but any future updates should be
    # a second "if version == 1.0" and so on. The first "if" needs to change the
    # version string, though.

    newini.write("# This config file was created %s by the update_ini script\n" % datetime.datetime.now())
    newini.write("# The original config files may be found in the %s directory\n\n" % backupdir)

    # reproduce everything before the first [section] verbatim
    section = re.match(r"(.*?)^\[", inistring, re.DOTALL | re.MULTILINE)
    if section !=None:
        newini.write(section.group(1))

    #[EMC] Section, change the version number
    all_sections.remove("EMC")
    section = re.search(r"\[EMC\](.+?)\n\[", inistring, re.DOTALL)
    if section: section = section.group(1)
    newini.write("[EMC]\n")
    if section != None:
        if version != "0.0":
            section = re.sub("VERSION (.+)", "VERSION = %s" % THIS_VERSION, section)
        else:
            newini.write("# The version string for this INI file.\n")
            newini.write("VERSION = %s\n" % THIS_VERSION)
        newini.write(section)
    else:
         newini.write("VERSION = %s\n" % THIS_VERSION)

    #These sections don't need any work.
    copysection("DISPLAY")
    copysection("FILTER")

    #[RS274NGC] Section, change FEATURES to separate entries
    all_sections.remove("RS274NGC")
    section = re.search(r"\[RS274NGC\](.+?)\n\[", inistring, re.DOTALL)
    if section: section = section.group(1)
    newini.write("\n[RS274NGC]\n")
    if section != None:
        features = ini.find('RS274NGC', 'FEATURES')
        if features != None:
            features = int(features)
            section = re.sub("FEATURES.*?\n", "", section)
            section += ("RETAIN_G43 = %s\n"   % ("1" if features & 0x1 else "0"))
            section += ("INI_VARS = %s\n"     % ("1" if features & 0x4 else "0"))
            section += ("HAL_PIN_VARS = %s\n" % ("1" if features & 0x8 else "0"))
            if features & 0x2: section  += ("OWORD_NARGS = 1\n" )
            if features & 0x10: section += ("NO_DOWNCASE_OWORD = 1\n" )
            if features & 0x20: section += ("OWORD_WARNONLY = 1\n" )
    section += "\n"
    newini.write(section)

    all_sections.remove("EMCMOT")
    section = re.search(r"\[EMCMOT\](.+?)\n\[", inistring, re.DOTALL)
    if section: section = section.group(1)
    newini.write("[EMCMOT]\n")
    section = re.sub("# Interval between tries to emcmot.*?\n", "", section)
    section = re.sub("COMM_WAIT.*?\n", "", section)
    newini.write(section)

    copysection("TASK")
    copysection("HAL")
    copysection("HALUI")

    # We need info from TRAJ to get KINS right

    joints = ini.find("TRAJ", "JOINTS")
    coordinates = ini.find("TRAJ", "COORDINATES").replace(" ","")
    if coordinates != None: joints = len(coordinates)
    if joints == None: joints = ini.find("TRAJ", "AXES")
    if joints == None: joints = "3"
    joints = int(joints)
    if coordinates == None: coordinates = "XYZABCUVW"

    coordinates = list(coordinates)

    # Search the Halfiles to find the kinematics.
    kins = None
    kinstype = None
    coords_entry = False
    if coordinates != "XYZABCUVW"[:joints]: coords_entry = True
    for halfile in halpaths:
        hal = open(os.path.join(os.path.dirname(filename), halfile), 'r')
        for line in hal.readlines():
            match = re.match(r'(?:#autoconverted|loadrt) +(\w+kins)', line)
            if match:
                kins = match.group(1)
                match = re.search(r'coordinates *= *(\w+)', line)
                if match:
                    coordinates = list(match.group(1))
                    coords_entry = 1
                match = re.search(r'kinstype *= *(\w+)', line)
                if kinstype: kinstype = match.group(1)
                break
        if kins: break
    if not kins: kins = "trivkins"

    #gantrykins and gentrivkins are gone, so need special treatment

    if  kins == "gantrykins":
        kins = "trivkins"
        kinstype = "BOTH"
        coords_entry = True
        for halfile in halpaths:
            hal = open(os.path.join(os.path.dirname(filename), halfile), 'r')
            for line in hal.readlines():
                match = re.match(r'setp +gantrykins.joint-(\d) +(\d)', line)
                if match:
                    j = int(match.group(1))
                    if j > joints: joints = j
                    a = int(match.group(2))
                    coordinates[j] = 'XYZABCUVW'[a]

    if kins == "gentrivkins":
        kins = "trivkins" #trivkins has the same defaults as gentrivkins

    # In JA [TRAJ] expects MAX_LINEAR_VELOCITY not MAX_VELOCITY
    all_sections.remove("TRAJ")
    section = re.search(r"\[TRAJ\](.+?)\n\[", inistring, re.DOTALL)
    if section: section = section.group(1)
    newini.write("\n[TRAJ]\n")
    if section != None:
        if not re.search("MAX_LINEAR_VELOCITY", section):
            if re.search("MAX_VELOCITY", section):
                section = re.sub("MAX_VELOCITY", "MAX_LINEAR_VELOCITY", section)
            else:
                mv = re.findall(r"MAX_VELOCITY[\s=]+(\d*(\.\d+)?)", inistring, re.MULTILINE)
                section = ("\n# this value based on fastest single axis" +
                          "\nMAX_LINEAR_VELOCITY = %s" % max(mv)[0] + section)
        if not re.search("DEFAULT_LINEAR_VELOCITY", section):
            section = re.sub("DEFAULT_VELOCITY", "DEFAULT_LINEAR_VELOCITY", section)
        if not re.search("MAX_LINEAR_ACCELERATION", section):
            section = re.sub("MAX_ACCELERATION", "MAX_LINEAR_ACCELERATION", section)
        if not re.search("DEFAULT_ACCELERATION", section):
            section = re.sub("DEFAULT_ACCELERATION", "DEFAULT_LINEAR_ACCELERATION", section)
        print("COORDINATES = %s\n" % ''.join(coordinates))
        section = re.sub("COORDINATES.*", "COORDINATES = %s" % ''.join(coordinates[: joints]), section)
        section = re.sub("CYCLE_TIME.*?\n", "", section)
        section = re.sub("AXES *=.*\n", "", section)
        newini.write(section)

    copysection("EMCIO")

    # Insert the new-fangled [KINS] section

    newini.write("\n\n[KINS]\n")
    newini.write("KINEMATICS = %s" % kins)
    if coords_entry: newini.write(" coordinates=%s" % ''.join(coordinates[: joints]))
    if kinstype: newini.write(" kinstype=%s" % kinstype)
    newini.write("\n")
    newini.write("#This is a best-guess at the number of joints, it should be checked\n")
    newini.write("JOINTS = %i\n" % joints)

    j = 0
    lock_mask = 0x0
    L2J={}
    while 1:
         # Search preferentially in "[JOINT_N] in case the file is part-converted already
        if re.search(r"^(\[JOINT_%i\])"%j, inistring, re.MULTILINE):
            if re.search(r"^(\[AXIS_%s\])" % "XYZABCUVW"[j], inistring, re.MULTILINE):
                copysection("AXIS_%s" % "XYZABCUVW"[j])
            #    copysection("JOINT_%i" % j)
            elif j < len(coordinates):
                newini.write("\n[AXIS_%s]\n" % coordinates[j])
                writeifexists(newini, "JOINT_%i" % j, "HOME")
                writeifexists(newini, "JOINT_%i" % j, "MIN_LIMIT")
                writeifexists(newini, "JOINT_%i" % j, "MAX_LIMIT")
                writeifexists(newini, "JOINT_%i" % j, "MAX_VELOCITY")
                writeifexists(newini, "JOINT_%i" % j, "MAX_ACCELERATION")
                copysection("[JOINT_%i]" % j)
        elif j < len(coordinates):
            # in this "elif" j is an index in to coordinates. 
            if coordinates[j] in L2J: # duplicate axis letter
                L2J[coordinates[j]].append(j) # = [L2J[coordinates[j]], j]
            else:
                L2J.update({coordinates[j] : [j]})
        elif j >= 9:
            break
        else:
            pass

        j += 1

    for L in list("XYZABCUVW"):
        if L in L2J:
            axisnum = "XYZABCUVW".index(L)
            newini.write("\n[AXIS_%s]\n" % L)
            writeifexists(newini, "AXIS_%i" % axisnum, "MIN_LIMIT")
            writeifexists(newini, "AXIS_%i" % axisnum, "MAX_LIMIT")
            writeifexists(newini, "AXIS_%i" % axisnum, "MAX_VELOCITY")
            writeifexists(newini, "AXIS_%i" % axisnum, "MAX_ACCELERATION")
            if ini.find("AXIS_%i" % j, "LOCKING_INDEXER"):
                lock_mask |= 1 << j
                newini.write("LOCKING_INDEXER_JOINT = %i\n" % j)
                
            hs = ini.find("AXIS_%i" % axisnum, "HOME_SEQUENCE")
            if hs == "-1" or hs == None: # -1 used to exclude a joint now we use no entry
                sequence = ""
            elif len(L2J[L]) > 1:  # tandem axis
                sequence = "HOME_SEQUENCE = -%s" % hs
            else:
                sequence = "HOME_SEQUENCE = %s" % hs
            for J in L2J[L]:
                # Take the coordinates index as the JOINT_Number
                newini.write("\n[JOINT_%i]" % J)
                section = re.search(r"\[AXIS_%i\](.+?)(\n\[|$)" % J, inistring, re.DOTALL)
                if not section:
                    section = re.search(r"\[AXIS_%i\](.+?)(\n\[|$)" % "XYZABCUVW".index(coordinates[J]), inistring, re.DOTALL)
                if section:
                    section = re.sub("HOME_SEQUENCE.*", sequence, section.group(1))
                    newini.write(section)
                    if not r'\[AXIS_%i\]' % axisnum in subs:
                        subs.update({r'\[AXIS_%i\]' % axisnum : '[JOINT_%i]' % J})
                        subs2.update({r'joint\.%i\.' % axisnum : 'joint.%i.' % J})
                    else:
                        subs.update({r'\[AXIS_%i\]' % J : '[JOINT_%i]' % J})
                else:
                    print("File parsing error, found an [AXIS_%i] section, but no content" % J)
                    exit()
    # We no longer need the [AXIS_N] data
    for j in range(0,8):
         if ("AXIS_%i" % j) in all_sections: all_sections.remove("AXIS_%i" % j)

    # If there were any custom sections, tag them on the end.
    while all_sections:
        copysection(all_sections[0])

    # and turn the locking mask into a string
    if lock_mask:
        lock_string = 'unlock_joints_mask=%i' % lock_mask
    else:
        lock_string = ""

    #That's the INI file done:
    newini.close()



    # Now change all the pin names etc in the linked HAL files.
    # Any machine can be jogged in world mode (in theory) but joint-mode jog-enable
    # is not auto-linked for safety.

if version == "$Revision$" or version < "1.0":
    
    subs.update({'axis.(.).active':'joint.\\1.active',
    'axis.(.).amp-enable-out':    'joint.\\1.amp-enable-out',
    'axis.(.).amp-fault-in':      'joint.\\1.amp-fault-in',
    'axis.(.).backlash-corr':     'joint.\\1.backlash-corr',
    'axis.(.).backlash-filt':     'joint.\\1.backlash-filt',
    'axis.(.).backlash-vel':      'joint.\\1.backlash-vel',
    'axis.(.).coarse-pos-cmd':    'joint.\\1.coarse-pos-cmd',
    'axis.(.).error':             'joint.\\1.error',
    'axis.(.).f-error':           'joint.\\1.f-error',
    'axis.(.).f-error-lim':       'joint.\\1.f-error-lim',
    'axis.(.).f-errored':         'joint.\\1.f-errored',
    'axis.(.).faulted':           'joint.\\1.faulted',
    'axis.(.).free-pos-cmd':      'joint.\\1.free-pos-cmd',
    'axis.(.).free-tp-enable':    'joint.\\1.free-tp-enable',
    'axis.(.).free-vel-lim':      'joint.\\1.free-vel-lim',
    'axis.(.).home-state':        'joint.\\1.home-state',
    'axis.(.).home-sw-in':        'joint.\\1.home-sw-in',
    'axis.(.).homed':             'joint.\\1.homed',
    'axis.(.).homing':            'joint.\\1.homing',
    'axis.(.).in-position':       'joint.\\1.in-position',
    'axis.(.).index-enable':      'joint.\\1.index-enable',
    'axis.(.).joint-pos-cmd':     'joint.\\1.pos-cmd',
    'axis.(.).joint-pos-fb':      'joint.\\1.pos-fb',
    'axis.(.).joint-vel-cmd':     'joint.\\1.vel-cmd',
    'axis.(.).kb-jog-active':     'joint.\\1.kb-jog-active',
    'axis.(.).motor-offset':      'joint.\\1.motor-offset',
    'axis.(.).motor-pos-cmd':     'joint.\\1.motor-pos-cmd',
    'axis.(.).motor-pos-fb':      'joint.\\1.motor-pos-fb',
    'axis.(.).neg-hard-limit':    'joint.\\1.neg-hard-limit',
    'axis.(.).neg-lim-sw-in':     'joint.\\1.neg-lim-sw-in',
    'axis.(.).pos-hard-limit':    'joint.\\1.pos-hard-limit',
    'axis.(.).pos-lim-sw-in':     'joint.\\1.pos-lim-sw-in',
    'axis.(.).wheel-jog-active':  'joint.\\1.wheel-jog-active',
    'axis.(.).unlock':            'joint.\\1.unlock',
    'axis.(.).is-unlocked':       'joint.\\1.is-unlocked',
    'axis.(.).jog-enable':        '@ax\\1@.jog-enable',
    'axis.(.).jog-counts':        'joint.\\1.jog-counts @ax\\1@.jog-counts',
    'axis.(.).jog-scale':         'joint.\\1.jog-scale @ax\\1@.jog-scale',
    'axis.(.).jog-vel-mode':      'joint.\\1.jog-vel-mode @ax\\1@.jog-vel-mode',
    'halui.axis.(.).pos-commanded':'halui.@ax\\1@.pos-commanded',
    'halui.axis.(.).pos-feedback':'halui.@ax\\1@.pos-feedback',
    'halui.axis.(.).pos-relative':'halui.@ax\\1@.pos-relative',
    'halui.joint.(.).is-selected':'halui.@ax\\1@.is-selected',
    'halui.jog.(.).analog':       'halui.@ax\\1@.analog',
    'halui.jog.(.).increment':    'halui.@ax\\1@.increment',
    'halui.jog.(.).increment-minus':'halui.@ax\\1@.increment-minus',
    'halui.jog.(.).increment-plus':'halui.@ax\\1@.increment-plus',
    'halui.jog.(.).minus':        'halui.@ax\\1@.minus',
    'halui.jog.(.).plus':         'halui.@ax\\1@.plus',
    'halui.joint.(.).select':     'halui.@ax\\1@.select',
    'halui.jog.selected.increment':'halui.axis.selected.increment',
    'halui.jog.selected.increment-minus':'halui.axis.selected.increment-minus',
    'halui.jog.selected.increment-plus':'halui.axis.selected.increment-plus',
    'halui.jog.selected.minus':   'halui.axis.selected.minus',
    'halui.jog.selected.plus':    'halui.axis.selected.plus',
    'halui.jog-deadband':         'halui.axis.jog-deadband',
    'halui.jog-speed':            'halui.axis.jog-speed',
    'halui.joint.selected':       'halui.joint.selected',
    'halui.joint.selected.is_homed':'halui.joint.selected.is-homed',
    'halui.joint.selected.on-soft-limit':'halui.joint.selected.on-soft-max-limit',
    'num_joints=[TRAJ]AXES':      'num_joints=[KINS]JOINTS',
    'loadrt(.+kins)':             'loadrt [KINS]KINEMATICS\n#autoconverted \\1',
    'shuttlexpress':              'shuttle',
    'shuttlepro':                 'shuttle',

    '(setp +gantrykins.+)':       '# \\1',
    '(.+servo_period_ns.+)':      '\\1 %s' % lock_string
    })

    # converts @axN@ pattern to axis.L and splits any malformed setp from mpg
    subs2.update({
    r'^\s*setp\s+(joint\S+)\s+(axis\S+)\s+(\S+)\s*$':
                                 'setp \\1 \\3\nsetp \\2 \\3\n',
    '@ax0@':                     'axis.x',
    '@ax1@':                     'axis.y',
    '@ax2@':                     'axis.z',
    '@ax3@':                     'axis.a',
    '@ax4@':                     'axis.b',
    '@ax5@':                     'axis.c',
    '@ax6@':                     'axis.u',
    '@ax7@':                     'axis.v',
    '@ax8@':                     'axis.w',
    })
    
if version < "1.1":
    subs.update({
        'motion.spindle-brake'                  : 'spindle.0.brake',
        'motion.spindle-forward'                : 'spindle.0.forward',
        'motion.spindle-index-enable'           : 'spindle.0.index-enable',
        'motion.spindle-inhibit'                : 'spindle.0.inhibit',
        'motion.spindle-on'                     : 'spindle.0.on',
        'motion.spindle-reverse'                : 'spindle.0.reverse',
        'motion.spindle-revs'                   : 'spindle.0.revs',
        'motion.spindle-speed-in'               : 'spindle.0.speed-in',
        'motion.spindle-speed-out'              : 'spindle.0.speed-out',
        'motion.spindle-speed-out-abs'          : 'spindle.0.speed-out-abs',
        'motion.spindle-speed-out-rps'          : 'spindle.0.speed-out-rps',
        'motion.spindle-speed-out-rps-abs'      : 'spindle.0.speed-out-rps-abs',
        'motion.spindle-orient-angle'           : 'spindle.0.orient-angle', 
        'motion.spindle-orient-mode'            : 'spindle.0.orient-mode',
        'motion.spindle-orient'                 : 'spindle.0.orient',
        'motion.spindle-is-oriented'            : 'spindle.0.is-oriented',
        'motion.spindle-orient-fault'           : 'spindle.0.orient-fault',
        'motion.spindle-locked'                 : 'spindle.0.locked',
        'motion.spindle-at-speed'               : 'spindle.0.at-speed',
        'halui.spindle.brake-is-on'             : 'halui.spindle.0.brake-is-on',
        'halui.spindle.brake-off'               : 'halui.spindle.0.brake-off',
        'halui.spindle.brake-on'                : 'halui.spindle.0.brake-on',
        'halui.spindle.decrease'                : 'halui.spindle.0.decrease',
        'halui.spindle.forward'                 : 'halui.spindle.0.forward',
        'halui.spindle.increase'                : 'halui.spindle.0.increase',
        'halui.spindle.is-on'                   : 'halui.spindle.0.is-on',
        'halui.spindle.reverse'                 : 'halui.spindle.0.reverse',
        'halui.spindle.runs-backward'           : 'halui.spindle.0.runs-backward',
        'halui.spindle.runs-forward'            : 'halui.spindle.0.runs-forward',
        'halui.spindle.start'                   : 'halui.spindle.0.start',
        'halui.spindle.stop'                    : 'halui.spindle.0.stop',
        'halui.spindle.override.count-enable'   : 'halui.spindle.0.override.count-enable',
        'halui.spindle.override.counts'         : 'halui.spindle.0.override.counts',
        'halui.spindle.override.decrease'       : 'halui.spindle.0.override.decrease',
        'halui.spindle.override.direct-value'   : 'halui.spindle.0.override.direct-value',
        'halui.spindle.override.increase'       : 'halui.spindle.0.override.increase',
        'halui.spindle.override.scale'          : 'halui.spindle.0.override.scale',
        'halui.spindle.override.value'          : 'halui.spindle.0.override.value'
        })

if subs == {}:
    print("""This file does not need converting, and furthermore execution
    should never have reached this spot""")
else:
    for halfile in halpaths:
        halstring = open(halfile,'r').read()
        for sub in subs:
            halstring = re.sub(sub, subs[sub], halstring, flags=re.M)
        for sub in subs2:
            halstring = re.sub(sub, subs2[sub], halstring, flags=re.M)
        newhal = open(halfile, 'w')
        newhal.write(halstring)
        newhal.close()

if force:
    shutil.rmtree(backupdir)
