#!/usr/bin/env python3
#
# Check the include style of exported headers
# Copyright (C) 2026 L. Toniolo
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2 or later.
#
# The build copies every SRCHEADERS entry into include/, so an exported header
# exists twice: the source under src/ and the copy every module compiles
# against. A quoted include searches the includer's own directory first, an
# angled include does not, so the two forms can reach different copies and both
# compile.
#
# An exported header travels to include/ and must take its siblings with it, so
# it includes them with quotes and finds the copies beside it wherever it ends
# up. Its implementation does the same, wanting the source next to it rather
# than a stale export. Everything else is a user, builds out of tree where only
# the exported copies exist, and uses angle brackets.
#
import sys
import os
import re
import getopt
import subprocess

error_on_warning = False

# The script lives in scripts/ and the sources are one level up, so it runs
# from anywhere.
topdir = os.path.normpath(os.path.join(os.path.dirname(os.path.realpath(__file__)), ".."))

SUFFIXES = (".c", ".cc", ".cpp", ".h", ".hh", ".comp")

# Headers whose own directory also holds code that merely uses them. Sitting
# beside the header says nothing there, so name the implementation and hold
# every other file in that directory to the angled form.
INTERFACES = {
    "src/emc/ini/inifile.hh": (
        "src/emc/ini/inifile.cc",
    ),
    "src/hal/hal.h": (
        "src/hal/hal_lib.c",
        "src/hal/hal_lib_extra.c",
        "src/hal/hal_lib_query.c",
    ),
}

RE_QUOTED = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.M)
RE_SRCHEADERS = re.compile(r"^SRCHEADERS\s*:=\s*\\\n((?:.*\\\n)*.*)$", re.M)


def usage():
    print("""Check the include style of exported headers.
Usage:
  include-style-check.py [-e] [-h] [file...]

Checks every source file under src/ when given no file, which is how CI runs
it. Named files are useful from a pre-commit hook.

Options:
  -e|--error    Treat findings as errors (--enforce is accepted as well)
  -h|--help     This message
""")
    sys.exit(2)


messages = []

#
# Collect messages
#
def pfind(path, lineno, msg):
    global messages
    messages.append((path, lineno, msg))

def flush_messages():
    kind = "error" if error_on_warning else "warning"
    for path, lineno, msg in messages:
        print("{}:{}: {}: {}".format(path, lineno, kind, msg))
    # Annotate the offending lines when running under CI
    if os.environ.get("GITHUB_ACTIONS"):
        for path, lineno, msg in messages:
            print("::{} file={},line={},title=Include style::{}".format(kind, path, lineno, msg))
    if not messages:
        return 0
    return 1 if error_on_warning else 0


def exported_headers():
    """Map each exported header's basename onto its path under src/, taken
    from the SRCHEADERS list the build installs into include/."""
    with open(os.path.join(topdir, "src", "Makefile"), encoding="utf-8", errors="replace") as f:
        m = RE_SRCHEADERS.search(f.read())
    if not m:
        print("No SRCHEADERS list found in src/Makefile", file=sys.stderr)
        sys.exit(2)
    headers = {}
    for line in m.group(1).split("\n"):
        entry = line.strip().rstrip("\\").strip()
        if entry:
            headers[os.path.basename(entry)] = "src/" + entry
    return headers


def tracked_files():
    """All tracked files under src/, and of those the ones worth reading."""
    try:
        out = subprocess.run(["git", "-C", topdir, "ls-files", "src"],
                             capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as err:
        print(err, file=sys.stderr)
        sys.exit(2)
    tracked = set(out.split("\n"))
    return tracked, sorted(f for f in tracked if f.endswith(SUFFIXES))


def check_quoted_includes(tracked, files, headers):
    exported = set(headers.values())
    for path in files:
        if path in exported:
            # An exported header is copied to include/ and has to keep finding
            # its siblings there, so it includes them with quotes.
            continue
        directory = os.path.dirname(path)
        with open(os.path.join(topdir, path), encoding="utf-8", errors="replace") as f:
            text = f.read()
        for m in RE_QUOTED.finditer(text):
            name = m.group(1)
            source = headers.get(os.path.basename(name))
            if source is None:
                continue    # not an exported header, nothing to say about it
            # A file of that name beside the includer is a different header
            # that happens to share the basename, not this one.
            local = os.path.normpath(os.path.join(directory, name))
            if local != source and local in tracked:
                continue
            if source in INTERFACES:
                if path in INTERFACES[source]:
                    continue    # the implementation, taking its own header
                why = "used from outside its implementation"
            elif directory == os.path.dirname(source):
                continue        # the implementation, taking the header beside it
            else:
                why = "exported by {}".format(os.path.dirname(source))
            lineno = text[:m.start()].count("\n") + 1
            pfind(path, lineno,
                  'include "{}" is {}, use <{}>'.format(name, why, os.path.basename(name)))


def main():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "eh", ["error", "enforce", "help"])
    except getopt.GetoptError as err:
        print(err, file=sys.stderr)
        usage()

    global error_on_warning
    for o, unused_a in opts:
        if o in ("-e", "--error", "--enforce"):
            error_on_warning = True
        elif o in ("-h", "--help"):
            usage()

    if os.environ.get("INCLUDE_STYLE_CHECK_ENFORCE"):
        error_on_warning = True

    # From here on we collect findings with pfind(). They get flushed when we
    # are done. The program's return value depends on whether findings are
    # treated as errors or not.
    headers = exported_headers()
    tracked, files = tracked_files()
    if args:
        # Named files, as a pre-commit hook would pass them. Anything outside
        # the set the whole-tree run covers has nothing to say about it.
        named = {os.path.relpath(os.path.abspath(a), topdir) for a in args}
        files = [f for f in files if f in named]
    check_quoted_includes(tracked, files, headers)

    return

if __name__ == "__main__":
    main()
    sys.exit(flush_messages())  # Exit value depends on findings being errors
