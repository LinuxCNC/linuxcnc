#!/usr/bin/env python3
# This file compares the entries in the component list (components.adoc) with the 
# available man pages and adds a tables for missing components.

from os import listdir
from os.path import isfile, join

man1_path = '../docs/html/man/man1'
man1_files = {f.replace('.1.html', '') for f in listdir(man1_path) if isfile(join(man1_path, f))}
man9_path = '../docs/html/man/man9'
man9_files = {f.replace('.9.html', '') for f in listdir(man9_path) if isfile(join(man9_path, f))}

file1 = open('../docs/src/hal/components.adoc', 'r')
section_switch = '[[sec:realtime-components]]'
doc1 = set()
doc9 = set()
components = [doc1,doc9]
comp_index = 0
for line in file1:
    if section_switch in line:
        comp_index = 1
    
    if line[0] == '|' and line[1] != '=':
        line = line.split('|')
        if line[1].strip() != '':
            components[comp_index].add(line[1].strip())
            #print(line[1].strip(), "-->", comp_index)

file1.close()

miss1 = man1_files.difference(doc1)
obs1 = doc1.difference(man1_files)
miss9 = man9_files.difference(doc9)
obs9 = doc9.difference(man9_files)

file2 = open('../docs/src/hal/components_gen1.adoc', 'w')
if len(miss1) > 0:
    file2.write('=== Not categorized (auto generated)\n')
    file2.write('[{tab_options}]\n|=======================\n')
    for i in sorted(miss1):
        file2.write('| ' + i + ' |||\n')
    file2.write('|=======================\n')
if len(obs1) > 0:
    file2.write('\n=== Obsolete (auto generated)\n')
    file2.write('[{tab_options}]\n|=======================\n')
    for i in sorted(obs1):
        file2.write('| ' + i + ' |||\n')
    file2.write('|=======================\n')
file2.close()

file3 = open('../docs/src/hal/components_gen9.adoc', 'w')
if len(miss9) > 0:
    file3.write('=== Not categorized (auto generated)\n')
    file3.write('[{tab_options}]\n|=======================\n')
    for i in sorted(miss9):
        file3.write('| ' + i + ' |||\n')
    file3.write('|=======================\n')
if len(obs9) > 0:
    file3.write('\n=== Obsolete (auto generated)\n')
    file3.write('[{tab_options}]\n|=======================\n')
    for i in sorted(obs9):
        file3.write('| ' + i + ' |||\n')
    file3.write('|=======================\n')
file3.close()

print('gen_complist: Added {} uncategorized and {} obsolete entries to hal component list (man1)'.format(len(miss1), len(obs1)))
print('gen_complist: Added {} uncategorized and {} obsolete entries to hal component list (man9)'.format(len(miss9), len(obs9)))