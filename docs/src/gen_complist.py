#!/usr/bin/env python3
# generate_complist()
#       Compares the entries in the component list (components.adoc) with the 
#       available man pages and adds tables for missing components.
# generate_links()
#       Generates a copy of components.adoc with added links to the man pages for the components.

from os import listdir
from os.path import isfile, join
import re
import sys

man1_path = '../docs/html/man/man1'
man1_files = {f.replace('.1.html', '') for f in listdir(man1_path) if isfile(join(man1_path, f))}
man9_path = '../docs/html/man/man9'
man9_files = {f.replace('.9.html', '') for f in listdir(man9_path) if isfile(join(man9_path, f))}
man = [man1_files,man9_files]
doc1 = set()
doc9 = set()
components = [doc1,doc9]
section_switch = '[[sec:realtime-components]]'
complist_path = '../docs/src/hal/components.adoc'

def generate_complist():
    file1 = open(complist_path, 'r')
    comp_index = 0
    for line in file1:
        if section_switch in line:
            comp_index = 1
        
        if line[0] == '|' and line[1] != '=':
            splitted = line.split('|')
            if splitted[1].strip() != '':
                if 'link:' in splitted[1]:
                    comp = re.search('\[.*\]', splitted[1]).group()
                else:
                    comp = splitted[1]
                components[comp_index].add(comp.strip('[] '))

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


def generate_links():
    file1 = open(complist_path, 'r')
    file1_links = open('../docs/src/hal/components_links.adoc', 'w')
    comp_index = 0
    manpage = '1'
    for line in file1:
        if section_switch in line:
            comp_index = 1
            manpage = '9'
        
        if line[0] == '|' and line[1] != '=':
            splitted = line.split('|')

            if 'link:' in splitted[1]:
                link = re.search('(?<=link:).*(?=\[)', splitted[1]).group()
                if not isfile(join('../docs/html/hal',link)):
                    print('broken link:', link)
            else:
                comp = splitted[1].strip(' ')
                if comp in man[comp_index]:
                    line = line.replace(comp, 'link:../man/man'+manpage+'/'+comp+'.'+manpage+'.html['+comp+']', 1)

        file1_links.write(line)

    file1.close()
    file1_links.close()

if __name__ == "__main__":

    if len(sys.argv) > 1:
        if sys.argv[1] == 'links':
            generate_links()

    else:
        generate_complist()