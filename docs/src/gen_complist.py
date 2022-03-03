#!/usr/bin/env python3
# generate_complist()
#       Compares the entries in the component list (components.adoc) with the 
#       available man pages and adds tables for missing components.
# generate_links()
#       Generates a copy of components.adoc with added links to the man pages for the components.

import os
import re
import sys

man1_path = '../docs/man/man1'
man1_files = {f.replace('.1', '') for f in os.listdir(man1_path) if os.path.isfile(os.path.join(man1_path, f))}
man9_path = '../docs/man/man9'
man9_files = {f.replace('.9', '') for f in os.listdir(man9_path) if os.path.isfile(os.path.join(man9_path, f))}
man = {'1':man1_files, '9':man9_files}
doc1 = set()
doc9 = set()
components = {'1':doc1, '9':doc9}
section_switch = '[[sec:realtime-components]]'
# complist_path = '../docs/src/hal/components.adoc'

def generate_complist(complist_path):
    file1 = open(complist_path, 'r')
    manpage = '1'
    for line in file1:
        if section_switch in line:
            manpage = '9'
        
        if line[0] == '|' and line[1] != '=':
            splitted = line.split('|')
            if splitted[1].strip() != '':
                if 'link:' in splitted[1]:
                    comp = re.search('\[.*\]', splitted[1]).group()
                else:
                    comp = splitted[1]
                components[manpage].add(comp.strip('[] '))

    file1.close()
    miss1 = man1_files.difference(doc1)
    obs1 = doc1.difference(man1_files)
    miss9 = man9_files.difference(doc9)
    obs9 = doc9.difference(man9_files)
    gen1_filename = '../docs/src/hal/components_gen1.adoc'
    file2 = open(gen1_filename, 'w')
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
    gen9_filename = '../docs/src/hal/components_gen9.adoc'
    file3 = open(gen9_filename, 'w')
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

    generate_links(gen1_filename, '1', False, True)
    generate_links(gen9_filename, '9', False, True)

    print('gen_complist: Added {} uncategorized and {} obsolete entries to hal component list (man1)'.format(len(miss1), len(obs1)))
    print('gen_complist: Added {} uncategorized and {} obsolete entries to hal component list (man9)'.format(len(miss9), len(obs9)))


def generate_links(filename, manpage='1', create_backup=True, add_descr=False):
    file = open(filename, 'r')
    file_links = []
    links_added = 0
    for line in file:
        if section_switch in line:
            manpage = '9'
        
        if line[0] == '|' and line[1] != '=':
            splitted = line.split('|')

            if 'link:' in splitted[1]:
                link = re.search('(?<=link:).*(?=\[)', splitted[1]).group()
                if not os.path.isfile(os.path.join('../docs/html/hal',link)):
                    print('gen_complist_link: Broken link:', link)
            else:
                comp = splitted[1].strip(' ')
                if comp in man[manpage]:
                    line = line.replace(comp, 'link:../man/man'+manpage+'/'+comp+'.'+manpage+'.html['+comp+']', 1)
                    links_added += 1
                    if add_descr:
                        splitted = line.split('|')
                        splitted[2] = extract_descr('../docs/man/man'+manpage+'/'+comp+'.'+manpage)\
                        .replace(comp, '',1).strip('\n -')
                        line = '|'.join(splitted)
        file_links.append(line)

    file.close()

    if links_added:
        if create_backup:
            os.rename(filename, filename+'~')        
        file = open(filename, 'w')
        for line in file_links:
            file.write(line)
        file.close()
        print('gen_complist_links: Added {} link(s) to {}'.format(links_added, filename))


def extract_descr(filename):
    file = open(filename, 'r')
    descr = ''
    in_descr = False
    
    for line in file:
        if '.SH NAME' in line or '.SH "NAME' in line:
            in_descr = True
        elif '.SH' in line:
            break
        elif in_descr:
            descr += line
    file.close()
    return re.sub(r'\\fB|\\fR|\\fI|\\', '', descr)

if __name__ == "__main__":

    if len(sys.argv) > 1:
        if 'links' in sys.argv:
            generate_links(sys.argv[1])
        else:
            generate_complist(sys.argv[1])
