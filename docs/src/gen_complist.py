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
man1_files = {f for f in os.listdir(man1_path) if os.path.isfile(os.path.join(man1_path, f))}
man9_path = '../docs/man/man9'
man9_files = {f for f in os.listdir(man9_path) if os.path.isfile(os.path.join(man9_path, f))}
man_files = man1_files.union(man9_files)
complist_doc = set()
miss_in_man = set()
# complist_path = '../docs/src/hal/components.adoc'

def generate_complist(complist_path):
    file1 = open(complist_path, 'r')
    for line in file1:
        if line[0] == '|' and line[1] != '=':
            splitted = line.split('|')
            if splitted[1].strip() != '':
                if 'link:' in splitted[1]:
                    link = re.search('(?<=link:).*(?=\\[)', splitted[1]).group()
                    comp_man = re.search("[a-zA-Z0-9-_\\.]+(?=\\.html)", link).group()
                    if os.path.isfile(os.path.join('../docs/html/hal',link)):
                        complist_doc.add(comp_man)
                    else:
                        print('gen_complist: Broken link:', link, file=sys.stderr)
                        miss_in_man.add(comp_man.split(".")[0])
                else:
                    print("gen_complist: Component \"" + splitted[1].strip() + "\" without link")
                    miss_in_man.add(splitted[1])
    file1.close()
    miss_in_list = man_files.difference(complist_doc)

    gen_filename = '../docs/src/hal/components_gen.adoc'
    file2 = open(gen_filename, 'w')
    if len(miss_in_list) > 0:
        file2.write('=== Not categorized (auto generated from man pages)\n')
        file2.write('[{tab_options}]\n|=======================\n')
        for i in sorted(miss_in_list):
            file2.write('| ' + i + ' |||\n')
        file2.write('|=======================\n')
    if len(miss_in_man) > 0:
        file2.write('\n=== Without man page or broken link (auto generated from component list)\n')
        file2.write('[{tab_options}]\n|=======================\n')
        for i in sorted(miss_in_man):
            file2.write('| ' + i + ' |||\n')
        file2.write('|=======================\n')
    file2.close()

    generate_links(gen_filename, False, True)
    print('gen_complist: Added {} uncategorized and {} potentially obsolete entry/entries to hal component list'.format(len(miss_in_list), len(miss_in_man)))


def generate_links(filename, create_backup=True, add_descr=False):
    file = open(filename, 'r')
    file_links = []
    links_added = 0
    for line in file:
        if line[0] == '|' and line[1] != '=':
            splitted = line.split('|')

            if 'link:' in splitted[1]:
                link = re.search('(?<=link:).*(?=\[)', splitted[1]).group()
                if not os.path.isfile(os.path.join('../docs/html/hal',link)):
                    print('gen_complist: Broken link:', link)
            else:
                comp_man = splitted[1].strip(' ')
                if comp_man in man_files:
                    comp, man = comp_man.split(".")
                    line = line.replace(comp_man, 'link:../man/man'+man+'/'+comp_man+'.html['+comp+']', 1)
                    links_added += 1
                    if add_descr:
                        splitted = line.split('|')
                        splitted[2] = extract_descr('../docs/man/man'+man+'/'+comp_man)\
                        .replace(comp + ' ', ' ',1).strip('\n -')
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
        print('gen_complist: Added {} link(s) to {}'.format(links_added, filename))


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
