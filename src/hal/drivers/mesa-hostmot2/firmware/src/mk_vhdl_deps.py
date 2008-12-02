#! /usr/bin/env python

# Copyright 2008, John Kasunich
# License: GPL2

import glob, re, os

# we are looking for the following patterns:

# 1) "entity<whitespace><identifier><whitespace>is"
# which means that this file defines entity <identifier>
re1 = re.compile("entity\s+(\w+)\s+is")

# 2) "package<whitespace><identifier>whitespace>is"
# which means that this file defines package <identifier>
re2 = re.compile("package\s+(\w+)\s+is")

# 3) ":<optwhitespace>entity<whitespace><identifier><whitespace>"
# which means that this file needs entity <identifier>
re3 = re.compile(":\s*entity\s+(\w+)\s+")

# 4) "use<whitespace>work.<identifier>."
# which means that this file needs package <identifier>
re4 = re.compile("use\s+work\.(\w+)\.")

n = 0
# these two dicts will contain 'item : file-that-defines-item' pairs
entities_defined = dict()
packages_defined = dict()
# these two dicts will contain 'file : set-of-things-it-needs' pairs
entities_needed = dict()
packages_needed = dict()

filenames = glob.glob('*.vhd')
for filename in filenames:
  f = open(filename)
  tmp_entities_needed = set()
  tmp_packages_needed = set()
  try:
    for line in f:
      # remove comments (anything after '--')
      line = line.split("--",1)[0]
      # after each regexp search, match.group(1) is <identifier>
      # convert <identifier> to lowercase since VHDL is case insensitive

      match = re1.search(line)
      if match:
	identifier = match.group(1).lower()
	entities_defined[identifier] = filename	

      match = re2.search(line)
      if match:
	identifier = match.group(1).lower()
	packages_defined[identifier] = filename	

      match = re3.search(line)
      if match:
	identifier = match.group(1).lower()
	tmp_entities_needed.add(identifier)

      match = re4.search(line)
      if match:
	identifier = match.group(1).lower()
	tmp_packages_needed.add(identifier)

  finally:
    f.close()
  # add the 'needed' sets for this file to the master dicts
  entities_needed[filename] = tmp_entities_needed
  packages_needed[filename] = tmp_packages_needed

# end of parsing - all info is in the four dicts

depfile = open("makefile.vhd.dep", "w")
for filename in filenames:
  # everything needed for this .vhd file will be added to this set
  files_needed = set()
  # first do entities
  for entity in entities_needed[filename]:
    if entity in entities_defined:
      files_needed.add(entities_defined[entity])
  # then packages
  for package in packages_needed[filename]:
    if package in packages_defined:
      files_needed.add(packages_defined[package])
  # Note that if either of the above ifs fail, then some entity or
  # package needed by 'filename' is not defined by any other source file.
  # If filename.vhd is compiled, the compile will fail.  We don't warn
  # about that here, because we don't know if the build will use every
  # file we are processing.

  # make output line, like: "file.dep : needed1.dep needed2.dep"
  filebase = os.path.splitext(filename)[0]
  depfile.write("%s.dep :" % filebase)
  for needname in files_needed:
    needbase = os.path.splitext(needname)[0]
    depfile.write(" %s.dep" % needbase)
  depfile.write("\n")
depfile.close()



