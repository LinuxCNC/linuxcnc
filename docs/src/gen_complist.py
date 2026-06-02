#!/usr/bin/env python3
# generate_complist()
#       Compares the entries in the component list (components.adoc) with the
#       available man pages and adds tables for missing components.
# generate_links()
#       Generates a copy of components.adoc with added links to the man pages for the components.

import os
import re
import sys

def write_if_changed(path, content):
    """Write content to path only if it differs from existing.
    Keeps mtime stable across no-op invocations so make stays idempotent."""
    try:
        with open(path, 'r') as f:
            if f.read() == content:
                return False
    except FileNotFoundError:
        pass
    with open(path, 'w') as f:
        f.write(content)
    return True

man1_path = '../docs/build/man/man1'
man1_files = {f for f in os.listdir(man1_path) if f[0] != '.' and os.path.isfile(os.path.join(man1_path, f))}
man9_path = '../docs/build/man/man9'
man9_files = {f for f in os.listdir(man9_path) if f[0] != '.' and os.path.isfile(os.path.join(man9_path, f))}
man_files = man1_files.union(man9_files)
complist_doc = set()
miss_in_man = set()

def link_target_exists(link):
    """Check whether the manpage source backing an asciidoctor link is
    present.  Compares against the troff filename set (man_files) rather
    than the rendered HTML, which does not exist when this script runs
    early in the build."""
    m = re.search(r'man[139]/([a-zA-Z0-9_.+-]+)\.html', link)
    if not m:
        return True
    return m.group(1) in man_files

def add_links(lines, add_descr):
    """Apply man-page link substitutions to the given lines (list of str,
    each ending in newline), return the result as a single string."""
    result = []
    for line in lines:
        if line and line[0] == '|' and line[1:2] != '=':
            splitted = line.split('|')
            if 'link:' in splitted[1]:
                link = re.search('(?<=link:).*(?=\\[)', splitted[1]).group()
                if not link_target_exists(link):
                    print('gen_complist: Broken link:', link)
            else:
                comp_man = splitted[1].strip(' ')
                if comp_man in man_files:
                    comp, man = comp_man.split(".")
                    line = line.replace(comp_man, 'link:../man/man'+man+'/'+comp_man+'.html['+comp+']', 1)
                    if add_descr:
                        splitted = line.split('|')
                        splitted[2] = extract_descr('../docs/build/man/man'+man+'/'+comp_man)\
                            .replace(comp + ' ', ' ', 1).strip('\n -')
                        line = '|'.join(splitted)
        result.append(line)
    return ''.join(result)

def generate_complist(complist_path):
    file1 = open(complist_path, 'r')
    for line in file1:
        if line[0] == '|' and line[1] != '=':
            splitted = line.split('|')
            if splitted[1].strip() != '':
                if 'link:' in splitted[1]:
                    link = re.search('(?<=link:).*(?=\\[)', splitted[1]).group()
                    comp_man = re.search("[a-zA-Z0-9-_\\.]+(?=\\.html)", link).group()
                    if link_target_exists(link):
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
    parts = []
    if len(miss_in_list) > 0:
        parts.append('\n== Not categorized (auto generated from man pages)\n')
        parts.append('[{tab_options}]\n|===\n')
        for i in sorted(miss_in_list):
            parts.append('| ' + i + ' |||\n')
        parts.append('|===\n')
    if len(miss_in_man) > 0:
        parts.append('\n== Without man page or broken link (auto generated from component list)\n')
        parts.append('[{tab_options}]\n|===\n')
        for i in sorted(miss_in_man):
            parts.append('| ' + i + ' |||\n')
        parts.append('|===\n')

    # Build links in memory rather than writing tables-only, reading back,
    # and writing tables+links.  The two-write pattern caused the on-disk
    # file to be rewritten on every run (final content always differs from
    # the tables-only intermediate), bumping mtime and re-triggering po4a.
    final_content = add_links(''.join(parts).splitlines(keepends=True),
                              add_descr=True)
    write_if_changed(gen_filename, final_content)
    print('gen_complist: Added {} uncategorized and {} potentially obsolete entry/entries to hal component list'.format(len(miss_in_list), len(miss_in_man)))

def generate_links(filename, create_backup=True, add_descr=False):
    """Public API: read filename, add links, write back. Kept for the
    'links' CLI subcommand path."""
    with open(filename, 'r') as f:
        lines = f.readlines()
    new_content = add_links(lines, add_descr)
    if create_backup:
        os.rename(filename, filename+'~')
    write_if_changed(filename, new_content)

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
