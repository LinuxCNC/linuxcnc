#!/usr/bin/env python
# coding: utf-8

'''
Will restore LinuxCNC files to end usage of nondeb_setup

usage :
    sudo restore_lcnc.py -c

    w/ or w/o argument c it will restore files and delete links

Created on 2016-10-08

@author: Fernand Veilleux
'''

import sys
import os

try :
    from lxml import etree
except :
    print('python-lxml required, command is in README.md')
    exit(1)

lcode = os.getenv('LANGUAGE', 'en')[0:2]
# print lcode

# cls = sys.argv.__len__() > 1
cls = True

if os.path.exists('/usr/share/linuxcnc/aux_gladevcp/NativeCAM/ncam.py') :
    cls = True

print 'wait, processing...'

find = os.popen("find /usr -name 'hal_pythonplugin.py'").read()
if find > '' :
    for s in find.split() :
        s = s.rstrip('\n')
        if not os.path.islink(s) :
            f = open(s).read()
            if cls :
                if f.find('from ncam import NCam') >= 0:
                    open(s, "w").write(f.replace('from ncam import NCam', ''))
                    print('"from ncam import NCam" removed from %s\n' % s)
            else :
                if f.find('from ncam import NCam') == -1:
                    open(s, "w").write('from ncam import NCam\n' + f)
                    print('"from ncam import NCam" added to %s\n' % s)
                else :
                    print('"from ncam import NCam" already exists in %s\n' % s)

        head, fn = os.path.split(s)

        fn = os.path.join(head, 'ncam.py')
        if os.path.islink(fn) and (fn <> os.path.join(os.getcwd(), 'ncam.py')) :
            os.remove(fn)
        if os.path.islink(fn) :
            if cls :
                os.remove(fn)
                print('removed link to ncam.py from %s\n' % head)
            else :
                print('link to ncam.py already exists in %s\n' % head)
        elif not cls :
            os.symlink(os.path.join(os.getcwd(), 'ncam.py'), fn)
            print('created link to ncam.py in %s\n' % head)

        fn = os.path.join(head, 'pref_edit.py')
        if os.path.islink(fn) and (fn <> os.path.join(os.getcwd(), 'pref_edit.py')) :
            os.remove(fn)
        if os.path.islink(fn) :
            if cls :
                os.remove(fn)
                print('removed link to pref_edit.py from %s\n' % head)
            else :
                print('link to pref_edit.py already exists in %s\n' % head)
        elif not cls :
            os.symlink(os.path.join(os.getcwd(), 'pref_edit.py'), fn)
            print('created link to pref_edit.py in %s\n' % head)

else :
    print 'Directory of "hal_pythonplugin.py" not found - EXITING'
    exit(2)

# create links to language files
fr_mo = '/usr/share/locale/fr/LC_MESSAGES/nativecam.mo'
if os.path.exists(fr_mo) :
    if os.path.islink(fr_mo) :
        if cls :
            os.remove(fr_mo)
else :
    os.symlink(os.path.join(os.getcwd(), 'locale/fr/LC_MESSAGES/nativecam.mo'), fr_mo)

de_mo = '/usr/share/locale/de/LC_MESSAGES/nativecam.mo'
if os.path.exists(de_mo) :
    if os.path.islink(de_mo) :
        if cls :
            os.remove(de_mo)
else :
    os.symlink(os.path.join(os.getcwd(), 'locale/de/LC_MESSAGES/nativecam.mo'), de_mo)


edited = False
find = os.popen("find /usr -name 'hal_python.xml'").read()
if find > '' :
    for s in find.split() :
        s = s.rstrip('\n')
        if not os.path.islink(s) :
            xml = etree.parse(s)
            root = xml.getroot()
            dest = root.find('glade-widget-classes')
            if cls :
                for n in dest.findall('glade-widget-class'):
                    if n.get('name') == 'NCam':
                        dest.remove(n)
                        edited = True
                        print('glade-widget-class named NCam removed from %s\n' % s)
                        break
            else :
                classfounded = False
                for n in dest.findall('glade-widget-class'):
                    if n.get('name') == 'NCam':
                        classfounded = True
                        print('glade-widget-class named NCam already exists in %s\n' % s)
                        break
                if not classfounded :
                    elem = etree.fromstring('''
<glade-widget-class name="NCam" generic-name="ncam" title="ncam">
            <properties>
                <property id="size" query="False" default="1" visible="False"/>
                <property id="spacing" query="False" default="0" visible="False"/>
                <property id="homogeneous" query="False" default="0" visible="False"/>
            </properties>
        </glade-widget-class> \n

''')
                    dest.insert(0, elem)
                    edited = True
                    print('glade-widget-class named NCam added to %s\n' % s)


            dest = root.find('glade-widget-group')
            if cls :
                for n in dest.findall('glade-widget-class-ref') :
                    if n.get('name') == 'NCam':
                        dest.remove(n)
                        edited = True
                        print('glade-widget-class-ref name NCam removed from %s\n' % s)
                        break
            else :
                classfounded = False
                for n in dest.findall('glade-widget-class-ref'):
                    if n.get('name') == 'NCam':
                        classfounded = True
                        print('glade-widget-class-ref named NCam already exists in %s' % s)
                        break
                if not classfounded :
                    dest.insert(0, etree.fromstring('<glade-widget-class-ref name="NCam"/>'))
                    edited = True
                    print('glade-widget-class-ref named NCam added to %s\n' % s)

            if edited :
                xml.write(s, pretty_print = True)
                print s, 'saved'

else :
    print('File "hal_python.xml" not found - EXITING')
    exit(3)

exit(0)
