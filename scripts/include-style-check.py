#!/usr/bin/env python3
#
# Check the include style of exported headers
# Copyright (C) 2026 L. Toniolo
#
# This program is free software; you can redistribute it and/or modify it under
# the terms of the GNU General Public License version 2 or later.
#
# The exported headers live in include/, which every compile searches. A quoted
# include searches the includer's own directory first and then falls back to
# the same search path, so a quoted include of an exported header compiles too,
# and the only thing it adds is the chance that a header of the same name beside
# the includer wins. Sources under src/ never sit beside include/, so they use
# angle brackets. The exported headers themselves include their siblings with
# quotes, which finds the file beside them wherever include/ is installed.
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

SUFFIXES = (".c", ".cc", ".cpp", ".h", ".hh", ".comp", ".icomp")

RE_QUOTED = re.compile(r'^[ \t]*#[ \t]*include[ \t]*"([^"]+)"', re.M)
# A .comp only becomes C below its ;; line.  Above it halcompile takes its own
# include directive, 'include "bla";', which is not a preprocessor line.
RE_COMP_QUOTED = re.compile(r'^[ \t]*include[ \t]*"([^"]+)"[ \t]*;', re.M)


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
    """The exported headers by name. They are the whole content of the include/
    directory, so the directory is the list."""
    return {fn for fn in os.listdir(os.path.join(topdir, "include"))
            if fn.endswith((".h", ".hh"))}


def tracked_files():
    """All tracked files under src/, and of those the ones worth reading."""
    try:
        out = subprocess.run(["git", "-C", topdir, "ls-files", "src"],
                             capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError) as err:
        print(err, file=sys.stderr)
        sys.exit(2)
    # A tracked path can be absent from the working tree, deleted but not yet
    # committed or mid-rebase. The compiler would not see it either, so drop it
    # rather than fail on the open() below.
    tracked = {f for f in out.split("\n")
               if f and os.path.isfile(os.path.join(topdir, f))}
    return tracked, sorted(f for f in tracked if f.endswith(SUFFIXES))


def check_quoted_includes(tracked, files, headers):
    for path in files:
        directory = os.path.dirname(path)
        with open(os.path.join(topdir, path), encoding="utf-8", errors="replace") as f:
            text = f.read()
        found = list(RE_QUOTED.finditer(text))
        if path.endswith((".comp", ".icomp")):
            head, sep, _ = text.partition("\n;;\n")
            found += RE_COMP_QUOTED.finditer(head if sep else text)
            found.sort(key=lambda m: m.start())
        for m in found:
            name = m.group(1)
            if os.path.basename(name) not in headers:
                continue    # not an exported header, nothing to say about it
            # A file of that name beside the includer is a different header
            # that happens to share the basename, not this one.
            local = os.path.normpath(os.path.join(directory, name))
            if local in tracked:
                continue
            lineno = text[:m.start()].count("\n") + 1
            pfind(path, lineno,
                  'include "{}" is an exported header, use <{}>'.format(name, os.path.basename(name)))


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
