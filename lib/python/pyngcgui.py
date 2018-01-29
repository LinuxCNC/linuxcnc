#!/usr/bin/env python

# Notes:
#   1) ini file items:
#      NGCGUI_PREAMBLE
#      NGCGUI_SUBFILE
#      NGCGUI_POSTAMBLE
#      NGCGUI_OPTIONS
#            nonew          disallow new tabs
#            noremove       disallow removal of tabs
#            noauto         don't automatically send result file
#            noexpand       (ngcgui used, not supported pyngcgui)
#            nom2           (no m2 terminator (use %))
#   2) To make pyngcgui embedded fit in small screen:
#       Try:
#         max_parms=10|20|30 (will reject otherwise valid subfiles)
#         image_width=240
#         reduce subroutine parm name lengths and/or comment string length

#------------------------------------------------------------------------------
# Copyright: 2013-6
# Author:    Dewey Garrett <dgarrett@panix.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#------------------------------------------------------------------------------
""" python classes to implement an ngcgui-like application

These ini file items are compatible with ngcgui.tcl:
  [DISPLAY]NGCGUI_PREAMBLE    single specifier
  [DISPLAY]NGCGUI_POSTAMBLE   single specifier
  [DISPLAY]NGCGUI_SUBFILE     multiples allowed, use "" for Custom tab
  [DISPLAY]NGCGUI_OPTIONS
           noremove           disallow tabpage removal
           nonew              disallow tabpage creation
           noiframe           don't show image in tabpage
           noauto             don't automatically send result file
  [DISPLAY]PROGRAM_PREFIX     subroutine path: start
  [RS274NGC]SUBROUTINE_PATH   subroutine path: middle
  [WIZARD]WIZARD_ROOT         subroutine path: end
  [DISPLAY]NGCGUI_FONT        not used
  [DISPLAY]TKPKG              not applicable
"""
from   types import * # IntType etc
import os
import sys
import re
import gtk
import getopt
import datetime
import subprocess
import linuxcnc
import hashlib
import gobject
import glob
import shutil
import popupkeyboard
import exceptions  # for debug printing
import traceback   # for debug printing
import hal         # notused except for debug
from gladevcp import hal_actions

g_ui_dir = linuxcnc.SHARE + "/linuxcnc"

# determine if glade interface designer is running
# in order to prevent connection of most signals
g_is_glade = False
if (     ('glade'        in sys.argv[0])
    and  ('gladevcp' not in sys.argv[0])):
    for d in os.environ['PATH'].split(':'):
        f = os.path.join(d,sys.argv[0])
        if (    os.path.isfile(f)
            and os.access(f, os.X_OK)):
            g_is_glade = True
            break
g_alive = not g_is_glade

import gettext
LOCALEDIR = linuxcnc.SHARE + "/locale"
gettext.install("linuxcnc", localedir=LOCALEDIR, unicode=True)

try:
    import pygtk
    pygtk.require('2.0')
except ImportError,msg:
    print('import pygtk failed: %s',msg)
    pass
#------------------------------------------------------------------------------
g_debug             = False
g_verbose           = False
g_nom2              = False # True for no m2 terminator (use %)
g_strict            = False # enforce additional subfile formatting requirements
g_tmode             = 0     # for development
g_entry_height      = 20    # default parm entry height
                            # (override for popupkeyboard)
g_big_height        = 35    # increased parm entry height value

g_image_width       = 320   # image size
g_image_height      = 240   # image size

g_check_interval    = 2 # periodic check (seconds)
g_label_id          = 0 # subroutine labels modifier when expanding in place
g_progname          = os.path.splitext(os.path.basename(__file__))[0]
g_dtfmt             = "%y%m%d:%H.%M.%S"

g_stat              = None # linuxcnc.stat  object
g_popkbd            = None # PopupKeyboard  object
g_candidate_files   = None # CandidateFiles object
g_send_function     = None # function object f(fname) return True for success
g_tab_controls_loc  ='top' # 'top' | 'bottom'

g_keyboardfile      = os.path.join(g_ui_dir,'popupkeyboard.ui')

g_control_font      = None
g_font_users        = []
g_auto_file_ct      = 1

INTERP_SUB_PARAMS = 30 # (1-based) conform to:
# src/emc/rs274ngc/interp_internal.hh:#define INTERP_SUB_PARAMS 30
g_max_parm       = INTERP_SUB_PARAMS
g_max_msg_len    = 500 # limit popup msg len for errant gcmc input

g_gcmc_exe = None
g_gcmc_funcname = 'tmpgcmc'
g_gcmc_id = 0

black_color   = gtk.gdk.color_parse('black')
white_color   = gtk.gdk.color_parse('white')
error_color   = gtk.gdk.color_parse('red')
green_color   = gtk.gdk.color_parse('green')
blue_color    = gtk.gdk.color_parse('blue')
yellow_color  = gtk.gdk.color_parse('yellow')
purple_color  = gtk.gdk.color_parse('purple')
feature_color = gtk.gdk.color_parse('lightslategray')

label_normal_color = gtk.gdk.color_parse('lightsteelblue2')
label_active_color = gtk.gdk.color_parse('ivory2')
base_entry_color   = gtk.gdk.color_parse('azure1')
fg_created_color   = gtk.gdk.color_parse('palegreen')
fg_multiple_color  = gtk.gdk.color_parse('cyan')
fg_normal_color    = black_color

bg_dvalue_color = gtk.gdk.color_parse('darkseagreen2')
#------------------------------------------------------------------------------

def exception_show(ename,detail,src=''):
    print('\n%s:' % src )
    print('Exception: %s' % ename )
    print('   detail: %s' % detail )
    if type(detail) == exceptions.ValueError:
        for x in detail:
            if type(x) in (StringType, UnicodeType):
                print('detail(s):',x)
            else:
                for y in x:
                    print('detail(d):',y,)
    elif type(detail) == StringType:
        print('detail(s):',detail)
    elif type(detail) == ListType:
        for x in detail:
            print('detail(l):',x)
    else:
        print(ename,detail)

    if g_debug:
        #print(sys.exc_info())
        print( traceback.format_exc())

def save_a_copy(fname,archive_dir='/tmp/old_ngc'):
    if fname is None:
        return
    try:
        if not os.path.exists(archive_dir):
            os.mkdir(archive_dir)
        shutil.copyfile(fname
              ,os.path.join(archive_dir,dt() + '_' + os.path.basename(fname)))
    except IOError,msg:
        print(_('save_a_copy: IOError copying file to %s') % archive_dir)
        print(msg)
    except Exception, detail:
        exception_show(Exception,detail,src='save_a_copy')
        print(traceback.format_exc())
        sys.exit(1)

def get_linuxcnc_ini_file():
    ps   = subprocess.Popen('ps -C linuxcncsvr --no-header -o args'.split(),
                             stdout=subprocess.PIPE
                           )
    p,e = ps.communicate()

    if ps.returncode:
        print(_('get_linuxcnc_ini_file: stdout= %s') % p)
        print(_('get_linuxcnc_ini_file: stderr= %s') % e)
        return None

    ans = p.split()[p.split().index('-ini')+1]
    return ans

def dummy_send(filename):
    return False # always fail

def default_send(filename):
    import gladevcp.hal_filechooser
    try:
        s = linuxcnc.stat().poll()
    except:
        user_message(mtype=gtk.MESSAGE_ERROR
            ,title=_('linuxcnc not running')
            ,msg = _('cannot send, linuxcnc not running'))
        return False
    try:
        fchooser = gladevcp.hal_filechooser.EMC_Action_Open()
        fchooser._hal_init()
        fchooser._load_file(filename)
        return True
    except:
        return False

def send_to_axis(filename): # return True for success
    # NB: file with errors may hang in axis gui
    s = subprocess.Popen(['axis-remote',filename]
                         ,stdout=subprocess.PIPE
                         ,stderr=subprocess.PIPE
                         )
    p,e = s.communicate()
    if s.returncode:
        print(_('%s:send_to_axis: stdout= %s') % (g_progname,p))
        print(_('%s:send_to_axis: stderr= %s') % (g_progname,e))
        return False
    if p: print(_('%s:send_to_axis: stdout= %s') % (g_progname,p))
    if e: print(_('%s:send_to_axis: stderr= %s') % (g_progname,e))
    return True

def file_save(fname,title_message='Save File'):
    start_dir = os.path.dirname(fname)
    if start_dir == '': start_dir = os.path.curdir
    fc = gtk.FileChooserDialog(title=title_message
       ,parent=None
       ,action=gtk.FILE_CHOOSER_ACTION_SAVE
       ,buttons=(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL
                ,gtk.STOCK_OK,     gtk.RESPONSE_OK
                )
       ,backend=None
       )
    fc.set_current_folder(start_dir)
    fc.set_do_overwrite_confirmation(True)
    filter = gtk.FileFilter()
    filter.set_name('NGC files')
    filter.add_pattern('*.ngc')
    filter.add_pattern('*.NGC')
    filter.add_pattern('*.nc')
    filter.add_pattern('*.NC')
    filter.add_pattern('*.gcmc')
    filter.add_pattern('*.GCMC')
    fc.add_filter(filter)
    fc.set_current_name(os.path.basename(fname)) # suggest name (for save)
    fname = None
    ans = fc.run()
    if ans == gtk.RESPONSE_OK:
        fname = fc.get_filename()
    elif ans == gtk.RESPONSE_CANCEL:
        print(_('file_save:canceled'))
    elif ans == gtk.RESPONSE_DELETE_EVENT: # window close
        print(_('file_save:window closed'))
    else:
        raise IOError,_('file_save:unexpected')
    fc.destroy()
    return(fname)

def is_comment(s):
    if s[0] == ';':      return bool(1) # ;xxx
    elif  s[0] == '(':
        if s[-2] == ')': return bool(1) # (yyy)
        else:            return bool(1) # (yyy)zzz  maybe bogus
    return bool(0)

def get_info_item(line):
    # expect line as unaltered line with whitespace
    l = line.translate(None,' \t').lower()
    r = re.search(r'^\(info:(.*)\)',l)
    if r:
        r = re.search(r'.*info:(.*)\)',line)
        if r: return r.group(1)
    return None

def check_sub_start(s):
    r = re.search(r'^o<(.*)>sub.*',s)
    if r:
        #print('check_sub_start:g0:',r.group(0))
        #print('check_sub_start:g1:',r.group(1))
        return r.group(1)
    return None

def check_sub_end(s):
    r = re.search(r'^o<(.*)>endsub.*',s)
    if r:
        #print('check_sub_end:g0:',r.group(0))
        #print('check_sub_end:g1:',r.group(1))
        return r.group(1)
    return None

def check_for_label(s):
    r = re.search(r'^o<(.*?)> *(sub|endsub).*',s)
    if r:
        return 'ignoreme' # do not include on expand

    r = re.search(r'^o<(.*?)> *(call).*',s)
    if r:
        return None # do not mod label on expand

    r = re.search(r'^o<(.*?)>.*',s)
    if r:
        return r.group(1) # make label unique on expand

    return None

def check_positional_parm_range(s,min,max):
    r = re.search(r'#([0-9]+)',s)
    if r: pnum = int(r.group(1))
    # here check is against system limit; g_max_parm applied elsewhere
    if r and (pnum <= INTERP_SUB_PARAMS):
        if pnum < min: min = pnum
        if pnum > max: max = pnum
        return pnum,min,max
    return None,None,None

def find_positional_parms(s):
    # requires original line (mixed case with whitespace)
    # find special association lines for positional parameters

    # The '*', '+', and '?' qualifiers are all greedy.
    #    Greedy <.*>  matches all of <H1>title</H1>
    # NonGreedy <.*?> matches the only first <H1>

    # case1  #<parmname>=#n (=defaultvalue comment_text)
    # case2  #<parmname>=#n (=defaultvalue)
    # case3  #<parmname>=#n (comment_text)
    # case4  #<parmname>=#n

    name    = None
    pnum    = None
    dvalue  = None
    comment = None
    s = s.expandtabs() # tabs to spaces

    r = re.search(
    r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *\(= *([0-9.+-]+[0-9.]*?) *(.*)\)'
               ,s,re.I)
    #case1   1name               2pnum          3dvalue             4comment

    if r is None: r=re.search(
    r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *\( *([0-9.+-]+)\)',s,re.I)
    #case2   1name               2pnum         3dvalue

    if r is None: r=re.search(
    r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *\((.*)\)',s,re.I)
    #case3   1name               2pnum       3comment

    if r is None: r=re.search(
    r' *# *<([a-z0-9_-]+)> *= *#([0-9]+) *$',s,re.I)
    #case4   1name               2pnum

    #   if r:
    #       for i in range(0,1+len(r.groups())):
    #           print('PARSE groups',len(r.groups()),i,r.group(i))

    if r:
        n = len(r.groups())
    if r and n >= 2:
        name = comment = r.group(1) # use name as comment if not specified
        pnum = int(r.group(2))
        # here check is against system limit; g_max_parm applied elsewhere
        if pnum > INTERP_SUB_PARAMS:
            return None,None,None,None
        if n == 3:
            if r.group(3)[0] == '=': dvalue = r.group(3)[1:]
            else:                    comment = r.group(3)[:]
        if n == 4:
            dvalue = r.group(3)
            if dvalue.find('.') >= 0:
                dvalue = float(dvalue)
            else:
                dvalue = int(dvalue)
            if r.group(4): comment = r.group(4)
        if n > 4:
            print('find_positional_parametrs unexpected n>4',s,)
            comment = r.group(4)
        if comment is None:
            print('find_positional_parameters:NOCOMMENT') # can't happen
            comment = ''
    return name,pnum,dvalue,comment

def user_message(title=""
                ,mtype=gtk.MESSAGE_INFO
                ,flags=gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT
                ,msg=None):
    if msg is None: return(None)
    if type(msg) == ListType:
        txt = "".join(msg)
    else:
        txt = msg

    vprint('USER_MESSAGE:\n%s' % txt)
    popup = gtk.MessageDialog(parent = None
          ,flags=flags
          ,type=mtype
          ,buttons = gtk.BUTTONS_OK
          ,message_format = txt
          )
    popup.set_title(title)
    result = popup.run()
    popup.destroy()
    return(result)

def dt():
    return(datetime.datetime.now().strftime(g_dtfmt))

def md5sum(fname):
    if not fname: return None
    return(hashlib.md5(open(fname, 'r').read()).hexdigest())

def find_image(fname):
    found = False
    for suffix in ('png','gif','jpg','pgm'):
        name = os.path.splitext(os.path.basename(fname))[0]
        dir  = os.path.dirname(fname)
        ifile = os.path.join(dir,name + '.' + suffix)
        if os.path.isfile(ifile):
            found = True
            break
    if not found: return None
    return ifile

def sized_image(ifile):
    twidth  = g_image_width
    theight = g_image_height
    img = gtk.Image()
    img.set_from_file(ifile)

    pixbuf  = img.get_pixbuf()
    iwidth  = pixbuf.get_width()  # image size
    iheight = pixbuf.get_height()
    scale      = min(float(twidth)/iwidth, float(theight)/iheight)
    #print('iw,ih %d,%d tw,th=%d,%d, scale=%f' % (
    #      iwidth,iheight,twidth,theight,scale))
    new_width  = int(scale*iwidth)
    new_height = int(scale*iheight)
    pixbuf = pixbuf.scale_simple(new_width,new_height
                                ,gtk.gdk.INTERP_BILINEAR)
    img.set_from_pixbuf(pixbuf)
    return(img)

def show_dir(x,tag=''):
    l = []
    for name in sorted(dir(x)):
        if  name[0:2] == '__': continue
        item = getattr(x,name)
        ty = type(item)
        if ty == MethodType:
            l.append('%-8s %s()' % ('0 Meth',name))
        elif ty == ListType:
            i = 0
            for v in item:
                try:
                    vnonewline = v[:-1] if v.endswith('\n') else v
                    l.append('%-8s %s[%2s] = %s' % ('2 List',name,i,vnonewline))
                    i += 1
                except:
                    l.append('xxx %s %s' % (name,str(item)))
        elif ty == DictionaryType:
            for k in sorted(item):
                l.append('%-8s %s[%2s] = %s' % ('3 Dict',name,k,item[k]))
        elif ty == BooleanType:
            l.append('%-8s %s = %s' % ('4 Bool',name,str(item)))
        elif ty == IntType:
            l.append('%-8s %s = %s' % ('5 Int',name,str(item)))
        elif ty == FloatType:
            l.append('%-8s %s = %s' % ('6 Float',name,str(item)))
        elif ty == StringType:
            l.append('%-8s %s = %s' % ('7 Str',name,item))
        else:
            s = str(item).split(' ')[0] + '>'
            s=item
            l.append('%-8s %s = %s' % ('1 Obj',name,s))

    print('\n')
    print('%s----------------------------------------------------------' % tag)
    for i in sorted(l):
        print(i)
    print('%s==========================================================' % tag)

def dprint(txt):
    if g_debug:
        print(':' + txt)

def vprint(txt):
    if g_verbose:
        print('::' + txt)

def spath_from_inifile(fname):
    if not fname:
        return []
    ini = linuxcnc.ini(fname)
    homedir = os.path.dirname(os.path.realpath(fname))
    # http://www.linuxcnc.org/docs/devel/html/config/ini_config.html
    l = []
    p = ini.find('DISPLAY','PROGRAM_PREFIX')
    if p:
        l = [p]
    p = ini.find('RS274NGC','SUBROUTINE_PATH')
    if p:
        newdirs = p.split(':')
        for dir in newdirs:
            # dont add duplicates
            if dir not in l:
                l.append(dir)
    p = ini.find('WIZARD','WIZARD_ROOT')
    if p:
        l.extend(p.split(':'))
    lfull = []
    for d in l:
        d = os.path.expanduser(d)
        if os.path.isabs(d):
            lfull.append(d)
        else:
            # relative path implies cwd is correct
            d2 = os.path.join(homedir,d)
            lfull.append(os.path.abspath(d2))
    if lfull:
        return lfull
    return []

def mpath_from_inifile(fname):
    if not fname:
        return None
    ini = linuxcnc.ini(ifname)
    homedir = os.path.dirname(os.path.abspath(fname))
    l = []
    p = ini.find('DISPLAY','PROGRAM_PREFIX')
    if p:
        l = [p]
    else:
        l = 'nc_files'
    p = ini.find('RS274NGC','USER_M_PATH')
    if p:
        l.extend(p.split(':'))
    lfull = []
    for d in l:
        if os.path.isabs(d):
            lfull.append(d)
        else:
            d2 = os.path.join(homedir,d)
            lfull.append(os.path.abspath(d2))
    if lfull:
        return lfull
    return None

def spath_from_files(pre_file,sub_files,pst_file):
    # when there is no ini file for path because
    #   linuxcnc not running
    # and
    #   no ini specified on cmd line
    l = []

    slist = []
    if type(sub_files) == StringType and sub_files:
        slist.append(sub_files)
    else:
        slist = sub_files

    for sub_file in slist:
        dir = os.path.dirname(os.path.abspath(sub_file))
        if dir not in l:
            l.append(dir)

    if pre_file:
        dir = os.path.dirname(os.path.abspath(pre_file))
        if dir not in l:
            l.append(dir)

    if pst_file:
        dir = os.path.dirname(os.path.abspath(pst_file))
        if dir not in l:
            l.append(dir)

    if l:
        return l
    return []

def long_name(name):
    if   name == 'pre':
        return 'Preamble'
    elif name == 'sub':
        return 'Subroutine'
    elif name == 'pst':
        return 'Postamble'
    else:
        return 'Unknown'

def show_parent(w,ct=0):
    if w is None:
        print('show_parent: None')
        return
    print('show_parent:',ct,w)
    if w.is_toplevel():
        print('TOP\n')
        return
    else:
        show_parent(w.get_parent(),ct+1)

def all_coords(iterable):
    ans = ''
    for t in iterable:
        ans = ans + '%7.3f' % t
    return ans

def show_position():
    g_stat.poll()
    print('POSITION-----------------------------------------------------')
    print('       ap',all_coords(g_stat.actual_position))
    print('        p',all_coords(g_stat.position))
    l = []
    p = g_stat.actual_position
    for i in range(9): l.append(p[i]
                               - g_stat.g5x_offset[i]
                               - g_stat.tool_offset[i]
                               )
    print('offset ap',all_coords(l))

    l = []
    p = g_stat.position
    for i in range(9): l.append(p[i]
                               - g_stat.g5x_offset[i]
                               - g_stat.tool_offset[i]
                               )
    print('offset  p',all_coords(l))
    print('POSITION=====================================================')

def coord_value(char):
    # offset calc from emc_interface.py (touchy et al)
    # char = 'x' | 'y' | ...
    # 'd' is for diameter
    c = char.lower()
    g_stat.poll()
    p = g_stat.position # tuple: (xvalue, yvalue, ...
    if (c == 'd'):
        if (1 & g_stat.axis_mask):
            # diam = 2 * x
            return (p[0] - g_stat.g5x_offset[0] - g_stat.tool_offset[0])* 2
        else:
            return 'xxx' # return a string that will convert with float()

    axno = 'xyzabcuvw'.find(c)
    if not ( (1 << axno) & g_stat.axis_mask ):
        return 'xxx' # return a string that will convert with float()
    return p[axno] - g_stat.g5x_offset[axno] - g_stat.tool_offset[axno]

def make_g_styles():

    dummylabel = gtk.Label()

    global g_lbl_style_default
    g_lbl_style_default   = dummylabel.get_style().copy()
    g_lbl_style_default.bg[gtk.STATE_NORMAL] = label_normal_color
    g_lbl_style_default.bg[gtk.STATE_ACTIVE] = label_active_color

    global g_lbl_style_created
    g_lbl_style_created  = dummylabel.get_style().copy()

    global g_lbl_style_multiple
    g_lbl_style_multiple = dummylabel.get_style().copy()

    g_lbl_style_multiple.bg[gtk.STATE_NORMAL] = feature_color
    g_lbl_style_multiple.bg[gtk.STATE_ACTIVE] = feature_color

    g_lbl_style_created.bg[gtk.STATE_NORMAL] = feature_color
    g_lbl_style_created.bg[gtk.STATE_ACTIVE] = feature_color

    del dummylabel


    dummyentry = gtk.Entry()

    global g_ent_style_normal
    g_ent_style_normal  = dummyentry.get_style().copy()

    global g_ent_style_default
    g_ent_style_default = dummyentry.get_style().copy()

    global g_ent_style_error
    g_ent_style_error   = dummyentry.get_style().copy()

    g_ent_style_normal.base[gtk.STATE_NORMAL]  = base_entry_color

    g_ent_style_default.base[gtk.STATE_NORMAL] = bg_dvalue_color

    g_ent_style_error.text[gtk.STATE_NORMAL]   = error_color
    g_ent_style_error.base[gtk.STATE_NORMAL]   = base_entry_color

    del dummyentry

def mod_font_by_category(obj,mode='control'):
    # currently mode = control (only)
    # touchy has 4 font categories: control,dro,error,listing
    if mode == 'control':
        font = g_control_font
    else:
        print('mod_font_by_category:unknown mode %s' % mode)
        return

    targetobj = None
    if type(obj) == type(gtk.Label()):
        targetobj = obj
    elif type(obj) == type(gtk.Entry()):
        targetobj = obj
    elif type(obj) == type(gtk.Button()):
        #gtk.Alignment object
        if isinstance(obj.child, gtk.Label):
            targetobj = obj.child
        elif isinstance(obj.child, gtk.Alignment):
            pass
        elif hasattr(obj,'modify_font'):
            targetobj = obj
        else:
            raise ValueError,'mod_font_by_category: no child'
            return
    else:
        raise ValueError,'mod_font_by_category: unsupported:',type(obj)
        return

    if targetobj is None:
        return
    if font is None:
        #print('mod_font_by_category:nofont available for %s' % mode)
        return # silently
    targetobj.modify_font(g_control_font)

    global g_font_users
    if targetobj not in g_font_users:
        g_font_users.append(targetobj)

def update_fonts(fontname):
    global g_control_font
    g_control_font = fontname
    for obj in g_font_users:
        mod_font_by_category(obj)

def clean_tmpgcmc(odir):
    if odir == "":
        odir = g_searchpath[0]
    savedir = os.path.join("/tmp", g_gcmc_funcname) # typ /tmp/tmpgcmc
    if not os.path.isdir(savedir):
        os.mkdir(savedir,0755)
    for f in glob.glob(os.path.join(odir,g_gcmc_funcname + "*.ngc")):
        # rename ng across file systems
        shutil.move(f,os.path.join(savedir,os.path.basename(f)))

def find_gcmc():
    global g_gcmc_exe # find on first request
    if g_gcmc_exe == "NOTFOUND": return False # earlier search failed
    if g_gcmc_exe is not None: return True    # already found

    for dir in os.environ["PATH"].split(os.pathsep):
        exe = os.path.join(dir,'gcmc')
        if os.path.isfile(exe):
            if os.access(exe,os.X_OK):
                clean_tmpgcmc("") # clean on first find_gcmc
                g_gcmc_exe = exe
                return True # success
    g_gcmc_exe = "NOTFOUND"
    user_message(mtype=gtk.MESSAGE_ERROR
                ,title=_('Error for:')
                ,msg = _('gcmc executable not available:'
                + '\nCheck path and permissions'))
    return False # fail

#-----------------------------------------------------------------------------

make_g_styles()


class CandidateDialog():
    """CandidateDialog: dialog with a treeview in a scrollwindow"""
    def __init__(self,ftype=''):
        self.ftype = ftype
        lname = long_name(self.ftype)
        title = "Choose %s file" % lname

        btns=(gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT
             ,gtk.STOCK_OK,     gtk.RESPONSE_ACCEPT)
        if ( (self.ftype == 'pre') or (self.ftype == 'pst') ):
            # RESPONSE_NO used to allow 'nofile' for 'pre','pst'
            btns = btns + ('No %s File' % lname, gtk.RESPONSE_NO)

        self.fdialog = gtk.Dialog(title=title
                     ,parent=None
                     ,flags=gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT
                     ,buttons=btns
                     )
        self.fdialog.set_size_request(600,600)

        scrollw = gtk.ScrolledWindow()
        scrollw.set_border_width(5)
        scrollw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_ALWAYS)
        scrollw.show()

        box = self.fdialog.get_content_area()
        box.pack_start(scrollw, True, True, 0)

        global g_candidate_files
        self.canfiles = g_candidate_files
        self.canfiles.refresh()
        self.treestore = g_candidate_files.treestore

        self.treeview = gtk.TreeView(self.treestore)
        if g_alive: self.treeview.connect('row-activated',self.row_activated)


        column0 = gtk.TreeViewColumn('Subroutine Directories')
        self.treeview.append_column(column0)
        cell0 = gtk.CellRendererText()
        column0.pack_start(cell0, True)
        column0.add_attribute(cell0, 'text', 0)

        column1 = gtk.TreeViewColumn('Hint')
        self.treeview.append_column(column1)
        cell1 = gtk.CellRendererText()
        column1.pack_start(cell1, True)
        column1.add_attribute(cell1, 'text', 1)

        column2 = gtk.TreeViewColumn('mtime')
        self.treeview.append_column(column2)
        cell2 = gtk.CellRendererText()
        column2.pack_start(cell2, True)
        column2.add_attribute(cell2, 'text', 2)

        scrollw.add_with_viewport(self.treeview)
        scrollw.show_all()

    def get_file_result(self):
        # return: (name,errmsg)
        try:
            (model,iter) = self.treeview.get_selection().get_selected()
        except AttributeError:
            return(None,'') # nothing selected
        if not iter:
            return(None,'')
        fname,status,mtime = self.canfiles.get_tree_data(iter)

        if os.path.isdir(fname):
            return(None,'') # cannot use a selected dir

        ok = True # contradict this
        if (self.ftype == 'pre') or (self.ftype == 'pst'):
            if status.find('not_a_subfile') >= 0: ok = True
            if status.find('Preempted')     >= 0: ok = False
        else:
            if status.find('not_a_subfile') >= 0: ok = False
            if status.find('not_allowed')   >= 0: ok = False
            if status.find('Preempted')     >= 0: ok = False

        if ok:
            return (fname,'')

        emsg = (_('The selected file is not usable\n'
                  'as a %s file\n'
                  '(%s)') % (long_name(self.ftype),status)
               )
        return('TRYAGAIN',emsg)

    def row_activated(self,tview,iter,column):
        self.fdialog.response(gtk.RESPONSE_ACCEPT)
        pass

    def run(self):
        return(self.fdialog.run())

    def destroy(self):
        self.fdialog.destroy()


class CandidateFiles():
    """CandidateFiles treestore for candidate files"""
    def __init__(self,dirlist):
        self.dirlist=dirlist
        self.treestore = gtk.TreeStore(str,str,str)
        self.tdict = {}
        self.make_tree()

    def refresh(self):
        # currently, just do over
        # potential to reread only files with modified mtimes
        self.__init__(self.dirlist)

    def make_tree(self):
        didx = 0
        flist = []
        for dir in self.dirlist:
            self.tdict[didx,] = dir
            # row must be a tuple or list containing as many items
            # as the number of columns
            try:
                mtime = datetime.datetime.fromtimestamp(os.path.getmtime(dir))
            except OSError,detail:
                print(_('%s:make_tree:%s' % (g_progname,detail) ))
                continue # try to skip this dir with message
            mtime = mtime.strftime(g_dtfmt) # truncate fractional seconds
            iter = self.treestore.append(None, [dir,"Directory",mtime])
            fidx = 0
            for f in ( sorted(glob.glob(os.path.join(dir,"*.ngc")))
                     + sorted(glob.glob(os.path.join(dir,"*.NGC")))
                     + sorted(glob.glob(os.path.join(dir,"*.gcmc")))
                     + sorted(glob.glob(os.path.join(dir,"*.GCMC")))
                     ):
                fname = os.path.basename(f)
                self.tdict[didx,fidx] = fname

                stat = ""
                fd = open(f)
                ftxt = fd.read()
                fd.close()

                if os.path.splitext(fname)[-1] in ['.gcmc','.GCMC']:
                    stat = '%sgcmc:ok' % stat

                if ftxt.find('not_a_subfile') >= 0:
                    stat = '%snot_a_subfile ' % stat
                if ftxt.find('(info:') >= 0:
                    stat = '%sngcgui-ok ' % stat
                if fname in flist:
                    stat = '%sPreempted ' % stat
                if ftxt.find('FEATURE') >= 0:
                    stat = '%snot_allowed ' % stat
                if stat == "":
                    stat = "?"
                if stat.find("Preempted") >= 0:
                    stat = "Preempted" # suppress ok

                flist.append(fname)
                mtime = datetime.datetime.fromtimestamp(os.path.getmtime(f))
                mtime = mtime.strftime(g_dtfmt) # truncate fractional seconds
                self.treestore.append(iter, [fname,stat,mtime])
                fidx += 1
            didx += 1

    def get_tree_data(self,iter):
        path = self.treestore.get_path(iter)
        if len(path) > 1:
            row,col = path
            dir = self.tdict[row,]
            fname = self.treestore.get_value(iter,0)
            status = self.treestore.get_value(iter,1)
            mtime = self.treestore.get_value(iter,2)
        else:
            dir = self.tdict[path]
            fname = ''
            status = ''
            mtime = ''
        return os.path.join(dir,fname),status,mtime


class LinuxcncInterface():
    """LinuxcncInterface: ini file and running linuxcnc data"""
    def __init__(self,cmdline_ini_file=''):
        self.lrunning = False
        self.ini_data = None
        self.subroutine_path = []
        self.user_m_path = None
        self.ini_file = None
        self.ngcgui_options = []
        self.editor = os.environ.get("VISUAL")
        use_ini_file = None

        l_ini_file = ''
        stat = linuxcnc.stat()


        try:
            global g_stat
            g_stat = linuxcnc.stat()
            g_stat.poll() # poll faults if linuxcnc not running
            self.lrunning = True
            l_ini_file = get_linuxcnc_ini_file()
        except linuxcnc.error,msg:
            g_stat = None
            print('INTFC:err:',msg)
            print('INTFC:' + _('Warning: linuxcnc not running'))

        print('%s:INTFC:linuxcnc running=%d' % (g_progname,self.lrunning))
        print('%s:INTFC:ini_file=<%s>' % (g_progname,l_ini_file))

        # cmdline_ini_file can be specified on cmdline and from intfc:
        # if neither ok: if no cmdline subfile, make custom page
        # if cmdonly ok
        # if runonly ok
        # if both    ok: warn message and continue

        if cmdline_ini_file:
            cmdline_spath = spath_from_inifile(cmdline_ini_file)
        if l_ini_file:
            l_spath = spath_from_inifile(l_ini_file)

        if not cmdline_ini_file and not l_ini_file:
            ini_file = None
            spath    = []
            #print('NEITHER')
        if not cmdline_ini_file and l_ini_file:
            ini_file = l_ini_file
            spath    = l_spath
            #print("OK running only <,",cmdline_ini_file,l_ini_file,">")
        if cmdline_ini_file and not l_ini_file:
            ini_file = cmdline_ini_file
            spath    = cmdline_spath
            #print('OK cmdline only')
        if cmdline_ini_file and l_ini_file:
            #print("BOTH ini file on both cmdline and running linuxcnc")
            msg = ""
            if os.path.abspath(cmdline_ini_file) != l_ini_file:
                ini_file = l_ini_file
                msg = (_('The ini file specified on cmdline') + ':\n'
                + os.path.abspath(cmdline_ini_file) + '\n\n'
                + _('is different from the one used by the running linuxcnc')
                + ':\n'
                + l_ini_file + '\n\n'
                )

            if cmdline_spath == l_spath:
                ini_file = cmdline_ini_file
                spath    = cmdline_spath
                msg = msg + _('Using cmd line ini file (same paths)')
            else:
                ini_file = l_ini_file
                spath    = l_spath
                msg = msg + _('Ignoring cmd line ini file (different paths)')

            user_message(mtype=gtk.MESSAGE_WARNING
                        ,title=_('Warning')
                        ,msg=msg
                        )

        if ini_file:
            self.ini_file = ini_file
            self.ini_data = linuxcnc.ini(self.ini_file)
            # get it again to avoid (unlikely) race
            self.subroutine_path = spath_from_inifile(ini_file)
            self.ngcgui_options = self.ini_data.find('DISPLAY','NGCGUI_OPTIONS')

            self.editor = (   self.editor
                           or self.ini_data.find('DISPLAY','EDITOR'))

        # create at startup, refresh as required
        global g_candidate_files
        g_candidate_files = CandidateFiles(self.get_subroutine_path())


    def addto_spath(self,pathtoadd):
        if type(pathtoadd) != ListType:
            raise ValueError,(
                'addto_spath: List required not: %s %s'
                % (pathtoadd,type(pathtoadd))
                )
        # dont add duplicates
        if pathtoadd not in self.subroutine_path:
            self.subroutine_path.extend(pathtoadd)

    def get_editor(self):
        return self.editor or 'gedit'

    def get_ini_file(self):
        return(self.ini_file)

    def get_subroutine_path(self):
        return(self.subroutine_path)

    def get_user_m_path(self):
        return(self.user_m_path)

    def find_file_in_path(self,fname):
        # return tuple:
        #               '', 'NULLFILE' if fname None or ''
        #            fname, 'NOPATH'   no path defined (eg no inifile)
        #    foundfilename, 'FOUND'    found in path
        #            fname, 'NOTFOUND' not in path (may exist)
        if not fname:
            return('','NULLFILE')
        if not self.subroutine_path:
            return(fname,'NOPATH')
        bname = os.path.basename(fname) # only basename used
        foundlist = []
        foundfilename = None
        for p in self.subroutine_path:
            f = os.path.join(p,bname)
            if os.path.isfile(f):
                if not foundfilename:
                    foundfilename = f #first one wins
                foundlist.append(f)

        if len(foundlist) > 1:
            print(_('find_file_in_path:Multiple Results: %s') % foundlist)
            print(_('      Search path: %s') % self.subroutine_path)
        if foundfilename:
            vprint('find_file_in_path:%s' % foundfilename)
            return(foundfilename,'FOUND')
        print('find_file_in_path<%s> NOTFOUND' % fname)
        return(fname,'NOTFOUND')

    def get_subfiles(self):
        if self.ini_data:
            #returns list
            return(self.ini_data.findall('DISPLAY','NGCGUI_SUBFILE'))
        else:
            return(None)

    def get_preamble(self):
        if self.ini_data:
            return(self.ini_data.find('DISPLAY','NGCGUI_PREAMBLE'))
        else:
            return(None)

    def get_postamble(self):
        if self.ini_data:
            return(self.ini_data.find('DISPLAY','NGCGUI_POSTAMBLE'))
        else:
            return(None)

    def get_font(self):
        if self.ini_data:
            return(self.ini_data.find('DISPLAY','NGCGUI_FONT'))
        else:
            return(None)

    def get_ngcgui_options(self):
        return(self.ngcgui_options or [])

    def get_gcmc_include_path(self):
        dirs = (self.ini_data.find('DISPLAY','GCMC_INCLUDE_PATH'))
        return(dirs)

    def get_program_prefix(self):
        if self.ini_data:
            dir = self.ini_data.find('DISPLAY','PROGRAM_PREFIX')
            dir = os.path.expanduser(dir)
            if not os.path.isabs(dir):
                # relative, base on inidir
                dir = os.path.join(os.path.dirname(self.ini_file),dir)
            return(dir)
        else:
            return(None)


class PreFile():
    """PreFile: preamble file data"""
    def __init__(self,thefile):
        self.pre_file = thefile
        self.read()

    def clear(self):
        self.pre_file = ''
        self.inputlines=[]

    def read(self):
        #print('PreFile read')
        self.md5 = None
        self.mtime = None
        self.inputlines = []
        if self.pre_file == "": return

        self.mtime = os.path.getmtime(self.pre_file)
        f = open(self.pre_file)
        for l in f.readlines():
            # dont include not_a_subfile lines
            if (l.find('not_a_subfile') < 0) and (l.strip() != ''):
                self.inputlines.append(l)
        f.close()
        self.md5 = md5sum(self.pre_file)


class PstFile():
    """PstFile: postamble file data"""
    def __init__(self,thefile):
        self.pst_file = thefile
        self.read()

    def clear(self):
        self.pst_file = ''
        self.inputlines = []

    def read(self):
        #print('PstFile read')
        self.md5 = None
        self.mtime = None
        self.inputlines = []

        if self.pst_file == "": return
        self.mtime = os.path.getmtime(self.pst_file)
        f = open(self.pst_file)
        for l in f.readlines():
            # dont include not_a_subfile lines
            if (l.find('not_a_subfile') < 0) and (l.strip() != ''):
                self.inputlines.append(l)
        f.close()
        self.md5 = md5sum(self.pst_file)


class SubFile():
    """SubFile: subfile data"""
    def __init__(self,thefile):
        self.sub_file = thefile
        self.min_num = sys.maxint
        self.max_num = 0
        self.pdict = {} # named items:   pdict[keyword] = value
        self.ndict = {} # ordinal items: ndict[idx] = (name,dvalue,comment)
        self.ldict = {} # label items:   ldict[lno] = thelabel
        self.pdict['info'] = ''
        self.pdict['lastparm'] = 0
        self.pdict['subname'] = ''
        self.inputlines = []
        self.errlist=[]
        self.md5 = None
        self.mtime = None
        if self.sub_file == '': return

        self.mtime = os.path.getmtime(self.sub_file)
        self.md5 = md5sum(self.sub_file)

        if os.path.splitext(self.sub_file)[-1] in ['.ngc','.NGC','.nc','.NC']:
            self.read_ngc()
        elif os.path.splitext(self.sub_file)[-1] in ['.gcmc','.GCMC']:
            self.read_gcmc()
        else:
            user_message(mtype=gtk.MESSAGE_ERROR
                    ,title=_('Unknown file suffix')
                    ,msg = _('Unknown suffix for: %s:')
                             % os.path.basename(self.sub_file)
                            )
            return

    def clear(self):
        self.sub_file = ''
        self.pdict = {}
        self.ndict = {}
        self.ldict = {}
        self.inputlines = []

    def flagerror(self,e):
        # accumulate erors from read() so entire file can be processed
        self.errlist.append(e)

    def specialcomments_ngc(self,s):
        if s.find(' FEATURE ')    >= 0 :
            self.flagerror(
            "Disallowed use of ngcgui generated file as Subfile")
        if s.find('not_a_subfile') >= 0 :
            self.flagerror(
            "marked (not_a_subfile)\nNot intended for use as a subfile")

    def re_read(self):
        if self.pdict.has_key('isgcmc'):
            self.read_gcmc()
        else:
            self.read_ngc()

    def read_ngc(self):

        thesubname = os.path.splitext(os.path.basename(self.sub_file))[0]

        f = open(self.sub_file)
        self.inputlines = [] # in case rereading
        for l in f.readlines():
            self.specialcomments_ngc(l) # for compat, check on unaltered line
            self.inputlines.append(l)
        idx = 1 # 1 based for labels ldict
        nextparm = 0
        subname = None
        endsubname = None
        for line in self.inputlines:
            # rs274: no whitespace, simplify with lowercase
            info = get_info_item(line) # check on unaltered line
            l = line.translate(None,' \t').lower()
            lineiscomment = is_comment(l)
            if info is not None: self.pdict['info'] = info
            sname = check_sub_start(l)
            if subname is not None and sname is not None:
                self.flagerror("Multiple subroutines in file not allowed")
            if subname is None and sname is not None:
                subname = sname
                if subname is not None and subname != thesubname:
                    self.flagerror("sub label "
                    "%s does not match subroutine file name" % thesubname)

            if endsubname is not None:
                if lineiscomment or (l.strip() == ''):
                    pass
                elif  l.find('m2') >= 0:
                    # linuxcnc ignores m2 after endsub in
                    # single-file subroutines
                    # mark as ignored here for use with expandsub option
                    self.inputlines[-1] = (';' + g_progname +
                                        ' ignoring: ' +  self.inputlines[-1])
                    pass
                else:
                    self.flagerror('file contains lines after subend:\n'
                                  '%s' % l)

            ename = check_sub_end(l)
            if subname is None and ename is not None:
                self.flagerror("endsub before sub %s" % ename)
            if subname is not None and ename is not None:
               endsubname = ename
               if endsubname != subname:
                   self.flagerror("endsubname different from subname")

            label = check_for_label(l)
            if label: self.ldict[idx] = label

            if (    subname is not None
                and endsubname is None
                and (not lineiscomment)):

                pparm,min,max= check_positional_parm_range(l
                               ,self.min_num,self.max_num)
                if pparm > g_max_parm:
                    self.flagerror(
                      _('parm #%s exceeds config limit on no. of parms= %d\n')
                        % (pparm,g_max_parm))
                if pparm:
                    self.min_num = min
                    self.max_num = max

                # blanks required for this, use line not l
                name,pnum,dvalue,comment = find_positional_parms(line)
                if name:
                    self.ndict[pnum] = (name,dvalue,comment)
                    # require parms in sequence to minimize user errors
                    nextparm = nextparm + 1
                    if g_strict:
                        if pnum != nextparm:
                            self.flagerror(
                                _('out of sequence positional parameter'
                                  '%d expected: %d')
                                % (pnum, nextparm))
                    while pnum > nextparm:
                        makename = "#"+str(nextparm)
                        self.ndict[nextparm] = makename,"",makename
                        nextparm = nextparm + 1
                    self.pdict['lastparm'] = pnum
            idx = idx + 1
        f.close()

        if    subname is None: self.flagerror(_('no sub found in file\n'))
        if endsubname is None: self.flagerror(_('no endsub found in file\n'))

        if g_strict:
            if nextparm == 0: self.flagerror(_('no subroutine parms found\n'))

        self.pdict['subname'] = subname
        if self.pdict['info'] == '':
            self.pdict['info'] = 'sub: '+str(subname)
        if self.errlist:
            user_message(mtype=gtk.MESSAGE_ERROR
                        ,title=_('Error for: %s ')
                                 % os.path.basename(self.sub_file)
                        ,msg = self.errlist)
            self.errlist.append('SUBERROR')
            raise ValueError,self.errlist

    def read_gcmc(self):
        self.gcmc_opts = [] # list of options for gcmc
        pnum = 0
        f = open(self.sub_file)
        for l in f.readlines():
            rinfo = re.search(r'^ *\/\/ *ngcgui *: *info: *(.*)' ,l)
            if rinfo:
                #print 'info read_gcmc:g1:',rinfo.group(1)
                self.pdict['info'] = rinfo.group(1) # last one wins
                continue

            ropt = re.search(r'^ *\/\/ *ngcgui *: *(-.*)$' ,l)
            if ropt:
                gopt = ropt.group(1)
                gopt = gopt.split("//")[0] ;# trailing comment
                gopt = gopt.split(";")[0]  ;# convenience
                gopt = gopt.strip()        ;# leading/trailing spaces
                self.gcmc_opts.append(gopt)
                continue

            name = None
            dvalue =  None
            comment = ''
            r3 = re.search(r'^ *\/\/ *ngcgui *: *(.*?) *= *(.*?) *\, *(.*?) *$', l)
            r2 = re.search(r'^ *\/\/ *ngcgui *: *(.*?) *= *(.*?) *$', l)
            r1 = re.search(r'^ *\\/\\/ *ngcgui *: *\(.*?\) *$', l)
            if r3:
                name = r3.group(1)
                dvalue = r3.group(2)
                comment = r3.group(3)
            elif r2:
                name = r2.group(1)
                dvalue = r2.group(2)
            elif r1:
                print 'r1-1 opt read_gcmc:g1:',r1.group(1)
                name = r1.group(1)

            if dvalue:
                # this is a convenience to make it simple to edit to
                # add a var without removing the semicolon
                #    xstart = 10;
                #    //ngcgui: xstart = 10;
                dvalue = dvalue.split(";")[0] # ignore all past a ;
            else:
                dvalue = ''

            if name:
                if comment is '':
                    comment = name
                pnum += 1
                self.ndict[pnum] = (name,dvalue,comment)

        self.pdict['isgcmc'] = True
        self.pdict['lastparm'] = pnum
        self.pdict['subname'] = os.path.splitext(os.path.basename(self.sub_file))[0]
        if self.pdict['info'] == '':
            self.pdict['info'] = 'gcmc: '+ self.pdict['subname']
        f.close()
        return True # ok

class FileSet():
    """FileSet: set of preamble,subfile,postamble files"""
    def __init__(self,pre_file
                     ,sub_file
                     ,pst_file
                ):
        # sub_file=='' is not an error, opens Custom
        self.pre_data = PreFile(pre_file)
        self.sub_data = SubFile(sub_file)
        self.pst_data = PstFile(pst_file)

class OneParmEntry():
    """OneParmEntry: one parameter labels and entry box"""
    def __init__(self,ltxt='ltxt' ,etxt='etxt' ,rtxt='rtxt'):

        self.box = gtk.HBox()

        self.ll  = gtk.Label()
        self.en  = gtk.Entry()
        self.lr  = gtk.Label()

        self.dv  = None

        ww = -1
        hh = g_entry_height

        self.ll.set_label(ltxt)
        self.ll.set_width_chars(2)
        self.ll.set_justify(gtk.JUSTIFY_RIGHT)
        self.ll.set_alignment(xalign=.90,yalign=0.5) # right aligned
        self.ll.set_size_request(ww,hh)

        self.en.set_text(etxt)
        self.en.set_width_chars(6)
        self.en.set_alignment(xalign=.90) # right aligned
        self.en.set_size_request(ww,hh)
        self.en.hide()

        #self.en.connect("button-press-event",self.grabit)
        if g_popkbd is not None:
            if g_alive: self.en.connect("button-press-event",self.popkeyboard)

        if g_alive: self.en.connect('changed', self.entry_changed) #-->w + txt

        self.lr.set_label(rtxt)
        self.lr.set_width_chars(0) # allow any width for compat with ngcgui
        self.lr.set_justify(gtk.JUSTIFY_LEFT)
        self.lr.set_alignment(xalign=0.2,yalign=0.5) # left aligned
        self.lr.set_size_request(ww,hh)
        self.lr.hide()
        mod_font_by_category(self.lr,'control')

        self.tbtns = gtk.HBox(homogeneous=0,spacing=2)
        self.tbtns.set_border_width(0)

        self.box.pack_start(self.tbtns, expand=0, fill=0, padding=0)

        self.tbtns.pack_start(self.ll, expand=0, fill=0, padding=0)
        self.tbtns.pack_start(self.en, expand=0, fill=0, padding=0)
        self.tbtns.pack_start(self.lr, expand=0, fill=0, padding=0)

    def grabit(self,*args,**kwargs):
        #print 'grabit',self,args,kwargs
        print '\ngrabit:can_get_focus:',self.en.get_can_focus()
        self.en.grab_focus()
        print 'grabit:has_focus',self.en.has_focus()
        print 'grabit: is_focus',self.en.is_focus()

    def popkeyboard(self,widget,v):
        origtxt = self.en.get_text()
        title = '#%s, <%s> %s' % (self.ll.get_text()
                                 ,self.en.get_text()
                                 ,self.lr.get_text()
                                 )
        self.en.set_text('')
        if g_popkbd.run(initial_value='',title=title):
            self.en.set_text(g_popkbd.get_result())
        else:
            # user canceled
            self.en.set_text(origtxt)

    def entry_changed(self,w):
        v = w.get_text().lower()
        if g_stat:
            r = re.search('[xyzabcuvwd]',v)
            if r:
                char = r.group(0)
                try:
                    w.set_text("%.4f" % coord_value(char))
                except TypeError:
                    pass
                except Exception, detail:
                    exception_show(Exception,detail,'entry_changed')
                    pass

        if v == '':
            w.set_style(g_ent_style_normal)
            return
        else:
            try:
                float(v)
                w.set_style(g_ent_style_normal)
            except ValueError:
                w.set_style(g_ent_style_error)
                return
        try:
            if (    (self.dv is not None)
                and (float(v) == float(self.dv)) ):
                w.set_style(g_ent_style_default)
                return
        except ValueError:
            pass
        w.set_style(g_ent_style_normal)
        return

    def getentry(self):
        return(self.en.get_text())

    def setentry(self,v):
        self.en.set_text(v)

    def clear_pentry(self):
        self.ll.set_text('')
        self.en.set_text('')
        self.lr.set_text('')
        self.ll.hide()
        self.en.hide()
        self.lr.hide()

    def make_pentry(self,ll,dvalue,lr,emode='initial'):
        # modes 'initial'
        #       'keep'
        self.dv = dvalue
        if dvalue is None:
            en = ''
        else:
            en = dvalue

        if ll is None: ll=''
        if lr is None: lr=''
        self.ll.set_text(str(ll))

        if emode == 'initial':
            self.en.set_text(str(en))

        # on reread, may be new parms with no text so use default
        # if (emode == 'keep') and (not self.en.get_text()):
        if (emode == 'keep') and (self.en.get_text() is None):
            self.en.set_text(str(en))

        self.lr.set_text(str(lr))
        if dvalue is None or dvalue == '':
            self.en.set_style(g_ent_style_normal) # normal (not a dvalue)
        else:
            self.en.set_style(g_ent_style_default) # a dvalue

        self.ll.show()
        self.en.show()
        self.lr.show()
        self.entry_changed(self.en)


class EntryFields():
    """EntryFields: Positional Parameters entry fields in a frame """
    def __init__(self,nparms=INTERP_SUB_PARAMS):
        if nparms > g_max_parm:
            raise ValueError,(_(
                  'EntryFields:nparms=%d g_max_parm=%d')
                  % (nparms,g_max_parm))
        self.ebox = gtk.Frame()
        self.ebox.set_shadow_type(gtk.SHADOW_ETCHED_IN)
        self.ebox.set_border_width(2)

        efbox = gtk.VBox()
        evb = gtk.VBox(homogeneous=0,spacing=2)
        xpositionalp = gtk.Label('Positional Parameters')
        xpositionalp.set_alignment(xalign=0.0,yalign=0.5) # left aligned
        epositionalp = gtk.EventBox()
        epositionalp.add(xpositionalp)
        epositionalp.modify_bg(gtk.STATE_NORMAL,label_normal_color)
        lpositionalp = gtk.Frame()
        lpositionalp.set_shadow_type(gtk.SHADOW_IN)
        lpositionalp.set_border_width(0)
        lpositionalp.add(epositionalp)


        self.boxofcolumns = gtk.HBox(homogeneous=0,spacing=2)

        evb.pack_start(lpositionalp,expand=0,fill=1,padding=0)
        evb.pack_start(self.boxofcolumns,   expand=1,fill=1,padding=4)

        efbox.pack_start(evb, expand=1,fill=1,padding=0)
        self.ebox.add(efbox)

        self.make_entryfields(nparms) # initialize for EntryFields

    def make_entryfields(self,nparms):
        self.no_of_entries = nparms
        # make VBoxes as required to accomodate entries
        # destroy them when starting over -- this occurs
        # when a OnePg is reused for a different subfile
        try:
            type(self.columnbox) # test for existence
            # destroy prior VBoxes packed in self.boxofcolumns
            for c in self.boxofcolumns.children():
                 self.boxofcolumns.remove(c)
                 c.destroy()
                 del(c)
        except AttributeError:
            # first-time: create initial VBox for entries
            self.columnbox = gtk.VBox(homogeneous=0,spacing=2)

        self.boxofcolumns.pack_start(self.columnbox)

        # try to use minimum height if less than 3 columns
        if nparms > 20:
            rowmax = 10
        else:
            rowmax  = int(nparms/2 + 0.5)

        self.pentries = {}
        row      = 0
        idx      = 1 # 1-based to agree with parm no.s
        for i in range(0,nparms):
            if row >= rowmax:
                row = 0
                # make a new VBox for next column of entries
                self.columnbox = gtk.VBox(homogeneous=0,spacing=2)
                self.boxofcolumns.pack_start(self.columnbox)
            self.pentries[idx] = OneParmEntry('','','')
            self.columnbox.pack_start(self.pentries[idx].box
                                     ,expand=0,fill=0,padding=0)
            row += 1
            idx += 1
        self.boxofcolumns.show_all()

    def getentry_byidx(self,idx):
        return(self.pentries[idx].getentry())

    def clear_pentry_byidx(self,idx):
        self.pentries[idx].clear_pentry()

    def make_pentry_byidx(self,idx,ll,en,lr,emode='initial'):
        self.pentries[idx].make_pentry(ll,en,lr,emode)

    def getstuff_byidx(self,idx):
        print("1getstuff idx=",idx)
        self.pentries[idx].getstuff()

    def get_box(self):
        return self.ebox

    def clear_parm_entries(self):
        for pidx in range(1,self.no_of_entries+1):
            self.clear_pentry_byidx(pidx)

    def set_parm_entries(self,parms,emode='initial'):
        lastpidx = 0
        for pidx in sorted(parms.sub_data.ndict):
            name,dvalue,comment = parms.sub_data.ndict[pidx]
            self.make_pentry_byidx(pidx
                                                 ,str(pidx)
                                                 ,dvalue
                                                 ,comment
                                                 ,emode
                                                 )
            lastpidx = pidx


class TestButtons():
    """TestButtons: debugging buttons"""
    def __init__(self,mypg):
        self.box  = gtk.HBox()
        self.mypg = mypg
        lbl       = gtk.Label('Debug:')
        lbl.set_alignment(xalign=0.9,yalign=0.5) # rt aligned
        self.box.pack_start(lbl,expand=0,fill=0,padding=2)
        for item in ('info'
                    ,'intfc'
                    ,'nset'
                    ,'nb'
                    ,'page'
                    ,'fset'
                    ,'pre'
                    ,'sub'
                    ,'pst'
                    ,'ent'
                    ,'cp'
                    ,'lcnc'
                    ,'hal'
                    ,'pos'
                    ,'glo'
                    ,'loc'
                    ,'tst'
                    ):
            button = gtk.Button(item)
            if g_alive: button.connect("clicked", self.btest, item)
            button.show_all()
            self.box.pack_start(button,expand=0,fill=0,padding=2)
        bclose = gtk.Button('Close')
        if g_alive: bclose.connect("clicked", lambda x: self.delete())
        self.box.pack_start(bclose,expand=0,fill=0,padding=2)

    def btest(self,widget,v):
        m = self.mypg
        if   v == 'info':
            p = m.nset
            print('INFO--------------------------------------------------')
            print('       sys.argv = %s' % sys.argv)
            print('            cwd = %s' % os.getcwd())
            print('       sys.path = %s' % sys.path)
            print('       ini_file = %s' % p.intfc.get_ini_file())
            print('      auto_file = %s' % p.auto_file)
            print('subroutine_path = %s' % p.intfc.get_subroutine_path())
            print('    user_m_path = %s' % p.intfc.get_user_m_path())
            print('       pre_file = %s' % p.intfc.get_preamble())
            print('        sublist = %s' % p.intfc.get_subfiles())
            print('       pst_file = %s' % p.intfc.get_postamble())
            print('  startpage_idx = %s' % p.startpage_idx)
            print('')
            print('      __file__  = %s' % __file__)
            print('g_send_function = %s' % g_send_function)
            print('       g_popkbd = %s' % g_popkbd)
            print('         g_stat = %s' % g_stat)
            print('     g_progname = %s' % g_progname)
            print('      g_verbose = %s' % g_verbose)
            print('        g_debug = %s' % g_debug)
            print('        g_tmode = %s' % g_tmode)
            print('     g_label_id = %s' % g_label_id)
        elif v  == 'ent':
            print('ENTRIES--------------------------------------------------')
            x = m.efields.pentries
            pmax = m.fset.sub_data.pdict['lastparm']
            print('efields.pentries[]')
            for pidx in range(1,pmax+1):
                print("%2d: %4s %-8s %-20s" % (pidx
                                           ,x[pidx].ll.get_text()
                                           ,x[pidx].en.get_text()
                                           ,x[pidx].lr.get_text()
                                           ))
            print('ENTRIES==================================================')
        elif v == 'intfc': d = m.nset.intfc;    show_dir(d,tag='intfc')
        elif v == 'page':
            d = m;               show_dir(d,tag='mypg')
            x=self.mypg.efields.pentries[1].en
            print 'x=',x
            print '            has_focus:',x.has_focus()
            print '             is_focus:',x.is_focus()
            print '        get_can_focus:',x.get_can_focus()
        elif v == 'pre':   d = m.fset.pre_data; show_dir(d,tag='pre_data')
        elif v == 'sub':   d = m.fset.sub_data; show_dir(d,tag='sub_data')
        elif v == 'pst':   d = m.fset.pst_data; show_dir(d,tag='pst_data')
        elif v == 'fset':  d = m.fset;          show_dir(d,tag='fset')
        elif v == 'nset':  d = m.nset;          show_dir(d,tag='nset')
        elif v == 'cp':    d = m.cpanel;        show_dir(d,tag='cpanel')
        elif v == 'loc':   show_dir(locals(),tag='locals')
        elif v == 'glo':   show_dir(globals(),tag='globals')
        elif v == 'lcnc':  show_dir(linuxcnc,tag='lcnc')
        elif v == 'hal':   show_dir(hal,tag='hal')
        elif v == 'pos':   show_position()
        elif v == 'tst':
            print('cpanel size:',m.cpanel.box.size_request())
            print('mtable size:',m.mtable.size_request())
        elif v == 'nb':
            print('NB--------------------------------------------------')
            for pno in range(m.nset.startpage_idx
                            ,m.mynb.get_n_pages()):
                npage = m.mynb.get_nth_page(pno)
                pg    = m.nset.pg_for_npage[npage]
                ltxt  = pg.the_lbl.get_text()
                print('%10s %s' % (ltxt,pg))
            print('NB==================================================')
        else: print('btest unknown:',v)

    def delete(self):
        gtk.main_quit()
        return False


class ControlPanel():
    """ControlPanel: Controls and image display"""
    def __init__(self
                ,mypg
                ,pre_file=''
                ,sub_file=''
                ,pst_file=''
                ):
        self.mypg = mypg
 
        frame = gtk.Frame()
        frame.set_shadow_type(gtk.SHADOW_ETCHED_IN)
        frame.set_border_width(2)
        self.box = frame
        
        cpbox  = gtk.VBox()
        # fixed width so it doesn't change when switching tabs
        # fixed height to allow room for buttons below image
        #cpbox.set_size_request(g_image_width,g_image_height)

        bw = 1
        bpre = gtk.Button(_('Preamble'))
        bpre.set_border_width(bw)
        mod_font_by_category(bpre)

        bsub = gtk.Button(_('Subfile'))
        bsub.set_border_width(bw)
        mod_font_by_category(bsub)

        bpst = gtk.Button(_('Postamble'))
        bpst.set_border_width(bw)
        mod_font_by_category(bpst)

        self.pre_entry = gtk.Entry()
        self.pre_entry.set_state(gtk.STATE_INSENSITIVE)

        self.sub_entry = gtk.Entry()
        self.sub_entry.set_state(gtk.STATE_INSENSITIVE)

        self.pst_entry = gtk.Entry()
        self.pst_entry.set_state(gtk.STATE_INSENSITIVE)

        chars=10

        self.pre_entry.set_width_chars(chars)
        self.pre_entry.set_alignment(xalign=0.1)
        self.pre_entry.set_text(os.path.basename(pre_file))
        if g_alive: self.pre_entry.connect("activate", self.file_choose, 'pre')

        self.sub_entry.set_width_chars(chars)
        self.sub_entry.set_alignment(xalign=0.1)
        self.sub_entry.set_text(os.path.basename(sub_file))
        if g_alive: self.sub_entry.connect("activate", self.file_choose, 'sub')

        self.pst_entry.set_width_chars(chars)
        self.pst_entry.set_alignment(xalign=0.1)
        self.pst_entry.set_text(os.path.basename(pst_file))
        if g_alive: self.pst_entry.connect("activate", self.file_choose, 'pst')

        xcontrol = gtk.Label('Controls')
        xcontrol.set_alignment(xalign=0.0,yalign=0.5) # left aligned
        econtrol = gtk.EventBox()
        econtrol.add(xcontrol)
        econtrol.modify_bg(gtk.STATE_NORMAL,label_normal_color)
        lcontrol= gtk.Frame()
        lcontrol.set_shadow_type(gtk.SHADOW_IN)
        lcontrol.set_border_width(0)
        lcontrol.add(econtrol)

        tfiles = gtk.Table(rows=3, columns=2, homogeneous=0)

        bx = gtk.FILL|gtk.EXPAND; by = 0

        tfiles.attach(bpre,0,1,0,1,xoptions=bx,yoptions=by)
        tfiles.attach(bsub,0,1,1,2,xoptions=bx,yoptions=by)
        tfiles.attach(bpst,0,1,2,3,xoptions=bx,yoptions=by)

        tfiles.attach(self.pre_entry,1,2,0,1,xoptions=bx,yoptions=by)
        tfiles.attach(self.sub_entry,1,2,1,2,xoptions=bx,yoptions=by)
        tfiles.attach(self.pst_entry,1,2,2,3,xoptions=bx,yoptions=by)

        if g_alive: bpre.connect("clicked", self.file_choose, 'pre')
        if g_alive: bsub.connect("clicked", self.file_choose, 'sub')
        if g_alive: bpst.connect("clicked", self.file_choose, 'pst')

        #bretain   = gtk.CheckButton('Retain values on Subfile read')
        self.bexpand   = gtk.CheckButton('Expand Subroutine')
        self.bexpand.set_active(self.mypg.expandsub)
        if g_alive: self.bexpand.connect("toggled", self.toggle_expandsub)

        self.bautosend = gtk.CheckButton('Autosend')
        self.bautosend.set_active(self.mypg.autosend)
        if g_alive: self.bautosend.connect("toggled", self.toggle_autosend)

        tchkbs = gtk.Table(rows=3, columns=1, homogeneous=0)
        bx = gtk.FILL|gtk.EXPAND; by = gtk.FILL|gtk.EXPAND
        #tchkbs.attach(bretain,  0,1,0,1,xoptions=bx,yoptions=by)
        tchkbs.attach(self.bexpand,  0,1,1,2,xoptions=bx,yoptions=by)

        nopts = self.mypg.nset.intfc.get_ngcgui_options()
        if (nopts is None) or ('noauto' not in nopts):
            tchkbs.attach(self.bautosend,0,1,2,3,xoptions=bx,yoptions=by)

        bw = 1

        bcreate   = gtk.Button(_('Create Feature'))
        bcreate.set_border_width(bw)
        if g_alive: bcreate.connect("clicked", lambda x: self.create_feature())
        mod_font_by_category(bcreate)

        bfinalize = gtk.Button(_('Finalize'))
        bfinalize.set_border_width(bw)
        if g_alive: bfinalize.connect("clicked"
                                     ,lambda x: self.finalize_features())
        mod_font_by_category(bfinalize)

        self.lfct = gtk.Label(str(mypg.feature_ct))
        self.lfct.set_alignment(xalign=0.9,yalign=0.5) # right aligned
        mod_font_by_category(self.lfct)

        lfctf = gtk.Frame()
        lfctf.set_shadow_type(gtk.SHADOW_IN)
        lfctf.set_border_width(2)
        lfctf.add(self.lfct)

        self.breread   = gtk.Button(_('Reread'))
        self.breread.set_border_width(bw)
        if g_alive: self.breread.connect("clicked"
                                        ,lambda x: self.reread_files())
        mod_font_by_category(self.breread)

        brestart  = gtk.Button(_('Restart'))
        brestart.set_border_width(bw)
        if g_alive: brestart.connect("clicked"
                                    ,lambda x: self.restart_features())
        mod_font_by_category(brestart)

        self.lmsg = gtk.Label(_('Ctrl-k for key shortcuts'))
        self.lmsg.set_alignment(xalign=0.05,yalign=0.5) # left aligned

        lmsgf = gtk.Frame()
        lmsgf.set_shadow_type(gtk.SHADOW_IN)
        lmsgf.set_border_width(2)
        lmsgf.add(self.lmsg)

        tactions = gtk.Table(rows=3, columns=3, homogeneous=1)
        bx = gtk.FILL|gtk.EXPAND; by = gtk.FILL|gtk.EXPAND
        tactions.attach(bcreate,  0,2,0,1,xoptions=bx,yoptions=by)
        tactions.attach(bfinalize,2,3,0,1,xoptions=bx,yoptions=by)

        # only if image (see below)
        # tactions.attach(self.breread ,0,1,1,2,xoptions=bx,yoptions=by)
        tactions.attach(brestart,   2,3,1,2,xoptions=bx,yoptions=by)

        bx = gtk.FILL|gtk.EXPAND; by = 0
        #tactions.attach(self.lmsg,0,3,2,3,xoptions=bx,yoptions=by)
        tactions.attach(lmsgf,0,3,2,3,xoptions=bx,yoptions=by)

        nopts = self.mypg.nset.intfc.get_ngcgui_options()
        image_file = find_image(sub_file)
        if image_file:
            img = sized_image(image_file)
        if (    (not image_file)
             or (nopts is not None and 'noiframe' in nopts)
             or mypg.imageoffpage
           ):
            # show all controls
            bx = gtk.FILL|gtk.EXPAND; by = gtk.FILL|gtk.EXPAND
            tactions.attach(self.breread, 0,1,1,2,xoptions=bx,yoptions=by)
            tactions.attach(lfctf,        1,2,1,2,xoptions=bx,yoptions=by)
            cpbox.pack_start(lcontrol,expand=0,fill=0,padding=0)
            cpbox.pack_start(tfiles,  expand=0,fill=0,padding=0)
            cpbox.pack_start(tchkbs,  expand=0,fill=0,padding=0)
            if image_file:
                self.separate_image(img,sub_file,show=False)
                mypg.imageoffpage = True
        else:
            bx = gtk.FILL|gtk.EXPAND; by = gtk.FILL|gtk.EXPAND
            tactions.attach(lfctf,  0,2,1,2,xoptions=bx,yoptions=by)
            # show image instead of controls
            if image_file:
                cpbox.pack_start(img,expand=0,fill=0,padding=0)
                mypg.imageoffpage = False
        cpbox.pack_start(tactions,expand=1,fill=1,padding=0)
        cpbox.show()
        frame.add(cpbox)

    def separate_image(self,img,fname='',show=True):
        self.mypg.imgw = gtk.Window(gtk.WINDOW_TOPLEVEL)
        w = self.mypg.imgw
        w.hide()
        w.iconify()
        w.set_title(os.path.basename(fname))
        w.add(img)
        if g_alive: w.connect("destroy",self.wdestroy)
        if show:
            w.show_all()
            w.deiconify()

    def wdestroy(self,widget):
        del self.mypg.imgw

    def set_message(self,msg):
        self.lmsg.set_label(msg)

    def reread_files(self):
        vprint('REREAD')
        # user can edit file and use button to reread it
        if self.mypg.sub_file == '':
            vprint('reread_files NULL subfile')
            return False
        self.mypg.fset.pre_data.read()
        self.mypg.fset.sub_data.re_read() # handle ngc or gcmc
        self.mypg.fset.pst_data.read()

        self.mypg.update_onepage('pre',self.mypg.pre_file)
        self.mypg.update_onepage('sub',self.mypg.sub_file)
        self.mypg.update_onepage('pst',self.mypg.pst_file)
        self.set_message(_('Reread files'))
        return True # success

    def restart_features(self):
        try:
            type(self.mypg.savesec) # test for existence
            self.mypg.savesec = []
        except AttributeError:
            pass
        self.mypg.feature_ct = 0
        self.lfct.set_label(str(self.mypg.feature_ct))
        self.mypg.savesec = []
        self.mypg.update_tab_label('default')
        self.set_message(_('Restart'))

    def toggle_autosend(self, widget):
        self.mypg.autosend = (0,1)[widget.get_active()]
        self.set_message(_('Toggle autosend %s ') % str(self.mypg.autosend))

    def toggle_expandsub(self, widget):
        self.mypg.expandsub = (0,1)[widget.get_active()]
        self.set_message(_('Toggle expandsub %s') % str(self.mypg.expandsub))

    def checkb_toggle(self, widget, var):
        print('1T',var,type(var))
        var = (0,1)[widget.get_active()]
        print('2T',var,type(var))

    def create_feature(self):
        m=self.mypg
        p=self.mypg.fset

        fpre,fprestat = m.nset.intfc.find_file_in_path(m.pre_file)
        fsub,fsubstat = m.nset.intfc.find_file_in_path(m.sub_file)
        fpst,fpststat = m.nset.intfc.find_file_in_path(m.pst_file)

        if fsubstat == 'NULLFILE':
            vprint('create_feature: NULLFILE')
            return
        # the test for NOPATH is for special cases
        if (   (fpre != p.pre_data.pre_file) and fprestat != 'NOPATH'
            or (fsub != p.sub_data.sub_file) and fsubstat != 'NOPATH'
            or (fpst != p.pst_data.pst_file) and fpststat != 'NOPATH'
            ):
            print('\nUSER changed filename entry without loading\n')

        try:
            type(self.mypg.savesec) # test for existence
        except AttributeError:
            self.mypg.savesec = []


        self.set_message(_('Create feature'))
        # update for current entry filenames
        p.pre_data = PreFile(m.pre_file) # may be ''
        p.sub_data = SubFile(m.sub_file) # error for ''
        p.pst_data = PstFile(m.pst_file) # may be ''

        if p.sub_data.pdict.has_key('isgcmc'):
            stat = self.savesection_gcmc()
        else:
            stat = self.savesection_ngc()

        if stat:
            if m.feature_ct > 0:
                self.mypg.update_tab_label('multiple')
            else:
                self.mypg.update_tab_label('created')

            m.feature_ct = m.feature_ct + 1
            self.lfct.set_label(str(m.feature_ct))

            self.set_message(_('Created Feature #%d') % m.feature_ct)
        else:
            #print "savesection fail"
            pass

    def savesection_ngc(self):
        m=self.mypg
        p=self.mypg.fset
        force_expand = False
        # if file not in path and got this far, force expand
        fname,stat = m.nset.intfc.find_file_in_path(m.sub_file)

        if stat == 'NOTFOUND':
            force_expand = True
            user_message(mtype=gtk.MESSAGE_INFO
                ,title=_('Expand Subroutine')
                ,msg=_('The selected file') + ':\n\n'
                + '%s\n\n'
                + _('is not in the linuxcnc path\n'
                    'Expanding in place.\n\n'
                    'Note: linuxcnc will fail if it calls\n'
                    'subfiles that are not in path\n')
                % fname)

        try:
            self.mypg.savesec.append(
                     SaveSection(mypg     = self.mypg
                                ,pre_info = p.pre_data
                                ,sub_info = p.sub_data
                                ,pst_info = p.pst_data
                                ,force_expand = force_expand
                                )
                     )
        except ValueError:
            dprint('SAVESECTION_ngc: failed')
        return True # success

    def savesection_gcmc(self):
        m=self.mypg
        p=self.mypg.fset
        intfc = self.mypg.nset.intfc

        global g_gcmc_exe
        if g_gcmc_exe is None:
            if not find_gcmc():
                return False ;# fail
        xcmd = []
        xcmd.append(g_gcmc_exe)

        global g_gcmc_funcname
        global g_gcmc_id
        g_gcmc_id += 1
        # gcmc chars in funcname: (allowed: [a-z0-9_-])
        funcname = "%s_%02d"%(g_gcmc_funcname,g_gcmc_id)

        p.sub_data.pdict['subname'] = funcname

        include_path = intfc.get_gcmc_include_path()
        if include_path is not None:
            for dir in include_path.split(":"):
                xcmd.append("--include")
                xcmd.append(os.path.expanduser(dir))
        # maybe: xcmd.append("--include")
        # maybe: xcmd.append(os.path.dirname(m.sub_file))
        # note: gcmc also adds the current directory
        #       to the search path as last entry.

        outdir = g_searchpath[0] # first in path
        ofile = os.path.join(outdir,funcname) + ".ngc"

        xcmd.append("--output")
        xcmd.append(ofile)

        xcmd.append('--gcode-function')
        xcmd.append(funcname)

        for opt in p.sub_data.gcmc_opts:
            splitopts = opt.split(' ')
            xcmd.append(str(splitopts[0]))
            if len(splitopts) > 1:
                xcmd.append(str(splitopts[1])) # presumes only one token


        for k in p.sub_data.ndict.keys():
            #print 'k=',k,p.sub_data.ndict[k]
            name,dvalue,comment = p.sub_data.ndict[k]
            # make all entry box values explicitly floating point
            try:
                fvalue = str(float(m.efields.pentries[k].getentry()))
            except ValueError:
                user_message(mtype=gtk.MESSAGE_ERROR
                    ,title='gcmc input ERROR'
                    ,msg=_('<%s> must be a number' % m.efields.pentries[k].getentry())
                    )
                return False ;# fail
            xcmd.append('--define=' + name + '=' + fvalue)

        xcmd.append(m.sub_file)
        print "xcmd=",xcmd
        e_message = ".*Runtime message\(\): *(.*)"
        e_warning = ".*Runtime warning\(\): *(.*)"
        e_error   = ".*Runtime error\(\): *(.*)"

        s = subprocess.Popen(xcmd
                             ,stdout=subprocess.PIPE
                             ,stderr=subprocess.PIPE
                             )
        sout,eout = s.communicate()
        m_txt = ""
        w_txt = ""
        e_txt = ""
        compile_txt = ""

        if eout:
            if (len(eout) > g_max_msg_len):
                # limit overlong, errant msgs
                eout = eout[0:g_max_msg_len] + "..."
            for line in eout.split("\n"):
                r_message = re.search(e_message,line)
                r_warning = re.search(e_warning,line)
                r_error = re.search(e_error,line)
                if r_message:
                    m_txt += r_message.group(1) + "\n"
                elif r_warning:
                    w_txt += r_warning.group(1) + "\n"
                elif r_error:
                    e_txt += r_error.group(1) + "\n"
                else:
                    compile_txt += line

        if m_txt != "":
            user_message(mtype=gtk.MESSAGE_INFO
                ,title='gcmc INFO'
                ,msg="gcmc File:\n%s\n\n%s"%(m.sub_file,m_txt)
                )
        if w_txt != "":
            user_message(mtype=gtk.MESSAGE_WARNING
                ,title='gcmc WARNING'
                ,msg="gcmc File:\n%s\n\n%s"%(m.sub_file,w_txt)
                )
        if e_txt != "":
            user_message(mtype=gtk.MESSAGE_ERROR
                ,title='gcmc ERROR'
                ,msg="gcmc File:\n%s\n\n%s"%(m.sub_file,e_txt)
                )
        if compile_txt != "":
            user_message(mtype=gtk.MESSAGE_ERROR
                ,title='gcmc Compile ERROR'
                ,msg="gcmc File:%s"%(compile_txt)
                )
        if s.returncode:
            return False ;# fail

        self.mypg.savesec.append(
                 SaveSection(mypg     = self.mypg
                            ,pre_info = p.pre_data
                            ,sub_info = p.sub_data
                            ,pst_info = p.pst_data
                            ,force_expand = False # never for gcmc
                            )
                 )
        return True # success

    def finalize_features(self):
        mypg = self.mypg
        nb   = self.mypg.mynb
        nset = self.mypg.nset
        if mypg.feature_ct <= 0:
            msg = _('No features specified on this page')
            self.set_message(msg)
            user_message(mtype=gtk.MESSAGE_WARNING
                    ,title='No Features'
                    ,msg=msg)
            return

        if len(mypg.savesec) == 0:
            msg = 'finalize_features: Unexpected: No features'
            self.set_message(_('No features'))
            raise ValueError,msg
            return
        txt = ''
        plist = []
        sequence = ""
        # these are in left-to-right order
        for pno in range(nset.startpage_idx,nb.get_n_pages()):
            npage = nb.get_nth_page(pno)
            #Using EventBox for tabpage labels: dont use get_tab_label_text()
            pg = nset.pg_for_npage[npage]
            ltxt = pg.the_lbl.get_text()
            howmany = len(pg.savesec)
            if howmany > 0:
                plist.append(pg)
                sequence = sequence + " " + ltxt
                txt = txt + "%s has %d features\n" % (ltxt,howmany)
        vprint(txt)

        if len(plist) > 1:
            msg = (_('Finalize all Tabs?\n\n'
                     'No:     Current page only\n'
                     'Yes:    All pages\n'
                     'Cancel: Nevermind\n\n'
                     'Order:'
                    )
                  + '\n<' + sequence + '>\n\n'
                     'You can Cancel and change the order with the\n'
                     'Forward and Back buttons\n'
                  )
            popup = gtk.Dialog(title='Page Selection'
                  ,parent=None
                  ,flags=gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT
                  ,buttons=(gtk.STOCK_NO,     gtk.RESPONSE_NO
                           ,gtk.STOCK_YES,    gtk.RESPONSE_YES
                           ,gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL
                           )
                  )
            finbox = popup.get_content_area()
            l = gtk.Label(msg)
            finbox.pack_start(l)
            popup.show_all()
            ans = popup.run()
            popup.destroy()
            if   ans == gtk.RESPONSE_YES:
                pass # use plist for all pages
            elif ans == gtk.RESPONSE_NO:
                pno = self.mypg.mynb.get_current_page()
                npage = nb.get_nth_page(pno)
                plist = [nset.pg_for_npage[npage]]
            elif (   ans == gtk.RESPONSE_CANCEL
                  or ans == gtk.REXPONSE_DELETE_EVENT): # window close
                return # do nothing
            else:
                raise ValueError, 'finalize_features:unknown ans<%d>'%ans

        # make a unique filename
        # (avoids problems with gremlin ignoring new file with same name)
        global g_auto_file_ct
        autoname = nset.auto_file
        dirname = os.path.realpath(os.path.dirname(autoname))
        basename = str(g_auto_file_ct) + "." + os.path.basename(autoname)
        tmpname  = os.path.join(dirname,basename)
        if os.path.exists(tmpname):
            os.remove(tmpname)
        # hack: alternate names (0,1) to force gremlin file loading
        #       and touchy filechooser updates
        g_auto_file_ct = (g_auto_file_ct + 1)%2
        basename = str(g_auto_file_ct) + "." + os.path.basename(autoname)
        tmpname  = os.path.join(dirname,basename)
        self.mypg.nset.last_file = tmpname

        savename = None
        f = open(tmpname,'w')
        nopts = self.mypg.nset.intfc.get_ngcgui_options()
        if (('nom2' in nopts) or g_nom2):
            f.write("%\n")
            f.write("(%s: nom2 option)\n" % g_progname)

        featurect = 0; features_total=0
        for pg in plist:
            features_total = features_total + len(pg.savesec)
        for pg in plist:
            ct = self.write_to_file(f,pg,featurect,features_total)
            featurect += ct
            pg.feature_ct = 0
            self.lfct.set_label(str(pg.feature_ct))
            pg.savesec = []

        if (('nom2' in nopts) or g_nom2):
            f.write("%\n")
        else:
            f.write("(%s: m2 line added) m2 (g54 activated)\n" % g_progname)
        f.close()

        user_must_save = True # disprove with send_function
        title_message = ''
        if self.mypg.autosend:
            if g_send_function(tmpname):
                user_must_save = False
                self.set_message(_('Finalize: Sent file'))
                save_a_copy(tmpname)
                print('%s:SENT: %s' % (g_progname,tmpname))
                print('%s:SENT:using: %s' % (g_progname,g_send_function.__name__))
            else:
                title_message = (
                  _('Sending file failed using function: <%s>, user must save')
                  % g_send_function.__name__)
                self.set_message(_('Finalize: Sent file failed'))
                print('%s:SAVEDFILE: after send failed: %s'
                     % (g_progname,tmpname))

        if user_must_save:
            fname  = os.path.abspath(nset.auto_file)
            if self.mypg.nset.last_file is not None:
                fname = self.mypg.nset.last_file # last user choice
            savename = file_save(fname,title_message) # user may change name
            if savename is not None:
                shutil.move(tmpname,savename)
                save_a_copy(savename)
                self.mypg.nset.last_file = savename

        for pg in plist:
            pg.cpanel.restart_features()
            pg.update_tab_label('default')

        global g_label_id
        g_label_id = 0 # reinitialize
        return

    def write_to_file(self,file,pg,featurect,features_total):
        ct = 0
        for i in range(0,len(pg.savesec) ):
            ct += 1
            for l in pg.savesec[i].sdata:
                if l.find("#<_feature:>") == 0:
                    file.write(
                      "(%s: feature line added) #<_feature:> = %d\n"\
                      % (g_progname,featurect))
                    featurect += 1
                    file.write(
                      "(%s: remaining_features line added) "
                      " #<_remaining_features:> = %d\n"\
                      % (g_progname,features_total - featurect))
                else:
                    file.write(l)
        return(ct)

    def file_choose(self,widget,ftype):
        mydiag = CandidateDialog(ftype=ftype)

        while True:
            response   = mydiag.run()
            fname,errmsg = mydiag.get_file_result()
            if   response == gtk.RESPONSE_ACCEPT:
                vprint('file_choose: ACCEPT')
                self.mypg.cpanel.set_message(_('file_choose ACCEPT'))
                pass
            elif response == gtk.RESPONSE_REJECT:
                self.mypg.cpanel.set_message(_('file_choose REJECT'))
                vprint('file_choose: REJECT')
                mydiag.destroy()
                return None
            elif response == gtk.RESPONSE_NO:
                self.mypg.cpanel.set_message(_('No File'))
                fname = 'nofile' # allow pre,pst nofile
                vprint('file_choose: No File')
            else:
                self.mypg.cpanel.set_message(_('file_choose OTHER'))
                mydiag.destroy()
                raise ValueError,_('file_choose OTHER %s') % str(response)
                return None

            if fname == 'TRYAGAIN':
                user_message(mtype=gtk.MESSAGE_INFO
                            ,title=_('Try Again')
                            ,msg=errmsg
                            )
                continue
            break
        mydiag.destroy()

        if   ftype == 'pre':
            self.mypg.fset.pre_file = fname
        elif ftype == 'sub':
             self.mypg.fset.sub_file = fname
        elif ftype == 'pst':
             self.mypg.fset.pst_file = fname
        else:
            raise ValueError,"file_choose ftype?",ftype

        # None for no file selected, null out field could be useful
        if not fname:
            self.mypg.cpanel.set_message(_('file_choose no file?'))
            return None

        if   ftype == 'pre':
            if fname == 'nofile':
                fname = ''
            self.pre_entry.set_text(os.path.basename(fname))
            self.mypg.update_onepage('pre',fname)
        elif ftype == 'sub':
            image_file = find_image(fname)
            if image_file:
                img = sized_image(image_file)
                self.separate_image(img,fname,show=True)
                self.mypg.imageoffpage = True
            if self.mypg.update_onepage('sub',fname):
                self.sub_entry.set_text(os.path.basename(fname))
        elif ftype == 'pst':
            if fname == 'nofile':
                fname = ''
            self.pst_entry.set_text(os.path.basename(fname))
            self.mypg.update_onepage('pst',fname)
        else:
            raise ValueError,'file_choose:Unexpected ftype <%s>' %ftype

        self.mypg.cpanel.set_message(_('Read %s') % os.path.basename(fname))
        return


class OnePg():
    """OnePg: ngcgui info for one tab page"""
    def __init__(self
                ,pre_file
                ,sub_file
                ,pst_file
                ,mynb
                ,nset
                ,imageoffpage=False
                ):

        self.imageoffpage   = imageoffpage # for clone of Custom pages
        self.garbagecollect = False
        self.key_enable     = False

        self.pre_file,stat = nset.intfc.find_file_in_path(pre_file)
        self.sub_file,stat = nset.intfc.find_file_in_path(sub_file)
        self.pst_file,stat = nset.intfc.find_file_in_path(pst_file)

        self.nset = nset
        self.mynb = mynb

        self.autosend = nset.autosend
        self.expandsub = nset.expandsub

        self.feature_ct = 0
        self.savesec = []

        self.cpanel = ControlPanel(mypg=self
                                  ,pre_file=self.pre_file
                                  ,sub_file=self.sub_file
                                  ,pst_file=self.pst_file
                                  )

        bw = 1

        #bremove = gtk.Button(_('Remove'))
        bremove = gtk.Button(stock=gtk.STOCK_DELETE)
        bremove.set_border_width(bw)
        if g_alive: bremove.connect("clicked", lambda x: self.remove_page())

        #bclone = gtk.Button(_('Clone'))
        bclone = gtk.Button(stock=gtk.STOCK_ADD)
        bclone.set_border_width(bw)
        if g_alive: bclone.connect("clicked", lambda x: self.clone_page())

        #bnew = gtk.Button(_('New'))
        bnew = gtk.Button(stock=gtk.STOCK_NEW)
        bnew.set_border_width(bw)
        if g_alive: bnew.connect("clicked", lambda x: self.new_empty_page())

        #bmoveleft = gtk.Button(_('<==Move'))
        bmoveleft = gtk.Button(stock=gtk.STOCK_GO_BACK,label='')
        bmoveleft.set_border_width(bw)
        if g_alive: bmoveleft.connect("clicked", lambda x: self.move_left())

        #bmoveright = gtk.Button(_('Move==>'))
        bmoveright = gtk.Button(stock=gtk.STOCK_GO_FORWARD,label='')
        bmoveright.set_border_width(bw)
        if g_alive: bmoveright.connect("clicked", lambda x: self.move_right())

        # stock buttons notwork with mod_font_by_category
        #mod_font_by_category(bremove)
        #mod_font_by_category(bclone)
        #mod_font_by_category(bnew)
        #mod_font_by_category(bmoveleft)
        #mod_font_by_category(bmoveright)

        tabarrange_buttons = gtk.HBox()        # main buttons

        self.mtable = gtk.Table(rows=1, columns=2, homogeneous=0)
        bx = gtk.FILL|gtk.EXPAND; by = 0
        no_of_parms = g_max_parm


        self.make_fileset()
        no_of_parms = self.fset.sub_data.pdict['lastparm']

        self.efields = EntryFields(no_of_parms) # uses MultipleParmEntries item

        self.fill_entrypage(emode='initial')

        bx = 0; by = gtk.FILL|gtk.EXPAND
        self.mtable.attach(self.cpanel.box, 0,1,0,1,xoptions=bx,yoptions=by)

        bx = gtk.FILL; by = gtk.FILL|gtk.EXPAND
        bx = gtk.FILL|gtk.EXPAND ; by = gtk.FILL|gtk.EXPAND
        entrystuff = self.efields.get_box()
        self.mtable.attach(entrystuff, 1,2,0,1,xoptions=bx,yoptions=by)

        tbtns = TestButtons(mypg=self) # TestButtons

        nopts = nset.intfc.get_ngcgui_options()

        if (nopts is None) or ('noremove' not in nopts):
            tabarrange_buttons.pack_start(bremove)

        if (nopts is None) or ('nonew' not in nopts):
            tabarrange_buttons.pack_start(bclone)
            tabarrange_buttons.pack_start(bnew)

        tabarrange_buttons.pack_start(bmoveleft)
        tabarrange_buttons.pack_start(bmoveright)

        op_box = gtk.VBox()

        if g_tab_controls_loc == 'top':
            op_box.pack_start(tabarrange_buttons,expand=0,fill=0,padding=0)
        elif g_tab_controls_loc == 'bottom':
            op_box.pack_end(tabarrange_buttons,expand=0,fill=0,padding=0)
        else:
            raise ValueError,(g_progname
                  + ' unknown tab_controls_loc %s' % g_tab_controls_loc)

        op_box.pack_start(self.linfof, expand=0,fill=0,padding=0)
        op_box.pack_start(self.mtable, expand=1,fill=1,padding=0)

        if g_debug:
            op_box.pack_end(tbtns.box,  expand=0,fill=0,padding=5)
        op_box.show_all()

        self.pgbox = gtk.EventBox()
        self.pgbox.add(op_box)
        self.pgbox.show_all()

        if g_alive: self.pgbox.connect('event',self.any_event)

        # establish size with max no of entries
        ww,hh = self.mtable.size_request()
        #print('size for mtable:',ww,hh)
        #self.mtable.set_size_request(ww,hh)

        lastpidx = self.fset.sub_data.pdict['lastparm']

        gobject.timeout_add_seconds(g_check_interval,self.periodic_check)


    def periodic_check(self):
        try:
            for i in ('pre','sub','pst'):
                o_entry = getattr(self.cpanel,i + '_entry')
                if o_entry.get_text().strip() == '': continue
                o_file  = getattr(self,      i + '_file')
                o_data  = getattr(self.fset, i + '_data')
                o_md5   = getattr(o_data,        'md5')
                o_mtime = getattr(o_data,        'mtime')
                if (    (o_mtime != None)
                    and (o_mtime == os.path.getmtime(o_file))):
                    state = o_entry.get_state()
                    o_entry.modify_text(state,black_color)
                    continue

                if (o_md5 != md5sum(o_file)):
                    #print('%s,%s>' % (o_md5,md5sum(o_file)))
                    #print(i,'CHANGED md5',o_file,o_md5)
                    state = o_entry.get_state()
                    o_entry.modify_text(state,purple_color)
                else:
                    #print(i,'SAME md5',o_file,o_md5)
                    o_entry.modify_text(gtk.STATE_NORMAL,black_color)
        except OSError, detail:
            print(_('%s:periodic_check:OSError:%s') % detail)
            pass # continue without checks after showing message
        except Exception, detail:
            exception_show(Exception,detail,'periodic_check')
            raise Exception, detail # reraise
        if self.garbagecollect:
            return False # False to norepeat (respond to del for self)
        return True      # True to repeat

    def any_event(self,widget,event):
        if   event.type == gtk.gdk.ENTER_NOTIFY:
            #widget.set_can_focus(True)
            self.key_enable = True
            #print('ENTER enable')
            return
        elif event.type == gtk.gdk.LEAVE_NOTIFY:
            #print "LEAVE can, is",widget.is_focus(),widget.get_can_focus(),'\n'
            if widget.get_can_focus():
                #widget.set_can_focus(False)
                self.key_enable = False
                #print('LEAVE disable')
            return
        elif event.type == gtk.gdk.EXPOSE:
            widget.grab_focus()
            return
        elif event.type == gtk.gdk.KEY_PRESS:
            if not self.key_enable:
                #print('IGNORE')
                return
            keyname = gtk.gdk.keyval_name(event.keyval)
            kl = keyname.lower()
            # ignore special keys (until they modify)
            if kl in ['alt_r','alt_l']         : return
            if kl in ['control_r','control_l'] : return
            if kl in ['shift_r','shift_l']     : return
            pre = ''
            if  event.state & gtk.gdk.CONTROL_MASK:
                pre = "Control-"
            elif event.state & gtk.gdk.MOD1_MASK:
                pre = "Alt-"
            elif event.state & gtk.gdk.SHIFT_MASK:
                pre = "Shift-"
            k = pre + keyname
            #print("%10s (%03d=%#2X)" % (k, event.keyval,event.keyval))
            self.handle_key(k)
            return False # allow other handlers

    def handle_key(self,k):
        if k == 'Control-d':
            self.make_fileset()
            self.fill_entrypage(emode='initial')
        if k == 'Control-a':
            self.cpanel.bautosend.clicked()
        if k == 'Control-#':
            self.cpanel.bexpand.clicked()
        if k == 'Control-k':
            self.show_special_keys()
        if k == 'Control-r':
            # was ctrl-p,P,r in ngcgui
            self.cpanel.breread.clicked()
        if k == 'Control-e':
            self.edit_any_file(self.nset.last_file,'last')
        if k == 'Control-E':
            self.cpanel.bexpand.clicked()
        if k == 'Control-u':
            self.edit_std_file('sub')
        if k == 'Control-U':
            self.edit_std_file('pre')
        #else:
        #    print('handle_key: k=',k)
        return False # False: allow more handlers

    def edit_any_file(self,fname,ftype=''):
        if not fname:
            user_message(mtype=gtk.MESSAGE_ERROR
                        ,title=_('No file')
                        ,msg=_('No %s file specified') % ftype
                        )
            return
        subprocess.Popen([self.nset.intfc.editor, fname])

    def edit_std_file(self,which):
        o_file  = getattr(self, which + '_file')
        self.edit_any_file(o_file,which)

    #NB some key bindings are claimed on touchy
    def show_special_keys(self):
        msg = []
        msg.append('Control-a  ' + _('Toggle autosend') + '\n')
        msg.append('Control-e  ' + _('Edit last result file') + '\n')
        msg.append('Control-E  ' + _('Toggle expandsubroutines') + '\n')
        msg.append('Control-d  ' + _('Set Entry defaults') + '\n')
        msg.append('Control-k  ' + _('Show keys (this)') + '\n')
        msg.append('Control-r  ' + _('Reread files') + '\n')
        msg.append('Control-u  ' + _('Edit sub file') + '\n')
        msg.append('Control-U  ' + _('Edit preamble file') + '\n')
        user_message(mtype=gtk.MESSAGE_INFO
                    ,title=_('Special Keys')
                    ,flags=0 #still MODAL ??
                    ,msg=msg)

    def set_page_label(self,lbl):
        self.lbl = lbl

    def save_onepage_tablabel(self,eb_lbl,the_lbl):
        self.eb_lbl  = eb_lbl
        self.the_lbl = the_lbl

    def update_tab_label(self,umode):
        if   umode == 'created':
            newcolor = fg_created_color
            newstyle = g_lbl_style_created
        elif umode == 'multiple':
            newcolor = fg_multiple_color
            newstyle = g_lbl_style_multiple
        elif umode == 'default':
            newcolor = fg_normal_color
            newstyle = g_lbl_style_default
        else:
            newstyle = g_lbl_style_default
            newcolor = fg_normal_color

        self.eb_lbl.set_style(newstyle)
        self.the_lbl.modify_fg(gtk.STATE_NORMAL, newcolor)
        self.the_lbl.modify_fg(gtk.STATE_ACTIVE, newcolor)

    def make_fileset(self):
        try:
            self.fset = FileSet(pre_file=self.pre_file
                               ,sub_file=self.sub_file
                               ,pst_file=self.pst_file
                               )
        except OSError,detail:
            print(_('%s:make_fileset:%s' % (g_progname,detail) ))
            raise OSError,detail # reraise

    def fill_entrypage(self,emode='initial'):
        self.efields.set_parm_entries(self.fset,emode)

        try:
            type(self.info_label) # test for existence
        except AttributeError:
            self.info_label = gtk.Label()
            self.linfof = gtk.Frame()
            self.linfof.set_shadow_type(gtk.SHADOW_IN)
            self.linfof.set_border_width(2)
            self.linfof.add(self.info_label)

        self.info_label.set_label(self.fset.sub_data.pdict['info'])
        self.info_label.set_alignment(xalign=0.0,yalign=0.5) # left aligned
        self.cpanel.set_message(_('Set Entry defaults'))

    def clear_entrypage(self):
        self.efields.clear_parm_entries()
        self.info_label.set_label('')

    def update_onepage(self,type,fname):
        vprint('UPDATE_PAGE  %s file=%s' % (type,fname))
        if   type == 'pre':
            foundname,stat = self.nset.intfc.find_file_in_path(fname)
            if stat == 'NOTFOUND':
                 self.clear_entries('pre')
                 return
            self.pre_file = foundname
            self.fset.pre_data = PreFile(self.pre_file)
        elif type == 'sub':
            foundname,stat = self.nset.intfc.find_file_in_path(fname)
            if stat == 'NOTFOUND':
                 self.clear_entries('sub')
                 return
            self.sub_file = foundname
            try:
                self.make_fileset()
                lastparm = self.fset.sub_data.pdict['lastparm']
                self.efields.make_entryfields(lastparm) # update_onepage
                self.fill_entrypage()
                self.info_label.set_label(self.fset.sub_data.pdict['info'])
                lbltxt = self.fset.sub_data.pdict['subname']
                lbltxt = self.nset.make_unique_tab_name(lbltxt)
                self.the_lbl.set_text(lbltxt)
                return True
            except Exception, detail:
                exception_show(Exception,detail,'update_onepage')
                return False
        elif type == 'pst':
            foundname,stat = self.nset.intfc.find_file_in_path(fname)
            if stat == 'NOTFOUND':
                 self.clear_entries('pst')
                 return
            self.pst_file = foundname
            self.fset.pst_data = PstFile(self.pst_file)
        else:
            raise ValueError,'update_onepage unexpected type <%s>' % type

        return

    def clear_entries(self,fmode):
        if   fmode == 'pre':
            self.pre_file = ''
            self.cpanel.pre_entry.set_text('')
            self.fset.pre_data.clear()
        elif fmode == 'sub':
            self.sub_file = ''
            self.cpanel.sub_entry.set_text('')
            self.clear_entrypage()
            self.fset.sub_data.clear()
        elif fmode == 'pst':
            self.pst_file = ''
            self.cpanel.pst_entry.set_text('')
            self.fset.pst_data.clear()
        else:
            raise ValueError,'clear_entries:unexpected fmode= %s' % fmode

    def move_left(self):
        page_idx = self.mynb.get_current_page()
        page_ct = self.mynb.get_n_pages()
        page = self.mynb.get_nth_page(page_idx)
        new_pg_idx = page_idx - 1
        if new_pg_idx < self.nset.startpage_idx:
            new_pg_idx = page_ct -1
        self.mynb.reorder_child(page,new_pg_idx%page_ct)

    def move_right(self):
        page_idx = self.mynb.get_current_page()
        page_ct = self.mynb.get_n_pages()
        page = self.mynb.get_nth_page(page_idx)
        new_pg_idx = (page_idx + 1)%page_ct
        if new_pg_idx < self.nset.startpage_idx:
            new_pg_idx = self.nset.startpage_idx
        self.mynb.reorder_child(page,new_pg_idx%page_ct)

    def clone_page(self):
        newpage = self.nset.add_page(self.pre_file
                                    ,self.sub_file
                                    ,self.pst_file
                                    ,self.imageoffpage #preserve for clone
                                    )
        for idx in self.efields.pentries:
            ev = self.efields.pentries[idx].getentry()
            newpage.efields.pentries[idx].setentry(ev)

    def new_empty_page(self):
        self.nset.add_page('','','')

    def remove_page(self):
        page_ct = self.mynb.get_n_pages()
        if page_ct - self.nset.startpage_idx == 1:
            user_message(mtype=gtk.MESSAGE_INFO
                ,title=_('Remove not allowed')
                ,msg=_('One tabpage must remain')
                )
        else:
            current_pno = self.mynb.get_current_page()
            npage = self.mynb.get_nth_page(current_pno)

            self.mynb.remove_page(current_pno)
            thispg = self.nset.pg_for_npage[npage]
            thispg.garbagecollect = True
            del thispg
            del npage



class NgcGui():
    """NgcGui: set of ngcgui OnePg items"""
    # make a set of pages in parent that depends on type(w)
    def __init__(self,w=None
                ,verbose=False
                ,debug=False
                ,noauto=False
                ,keyboardfile='' # None | ['default'|'yes'] | fullfilename
                ,tmode=0
                ,send_function=default_send # prototype: (fname)
                ,ini_file=''
                ,auto_file=''
                ,pre_file=''
                ,sub_files=''
                ,pst_file=''
                ,tab_controls_loc='top'  # option for touchy
                ,control_font=None       # option for touchy
                ,gtk_theme_name=None     # option for touchy
                ,max_parm=None           # for small display, reject some subs
                ,image_width=None        # for small display
                ):

        global g_send_function;    g_send_function    = send_function
        global g_tmode;            g_tmode            = tmode
        global g_verbose;          g_verbose          = verbose
        global g_debug;            g_debug            = debug

        global g_tab_controls_loc; g_tab_controls_loc = tab_controls_loc
        global g_control_font;     g_control_font     = control_font

        try:
            type(g_send_function) # test existence
            if g_send_function == None:
                g_send_function = dummy_send
        except AttributeError:
            print 'INVALID send_function, using dummy'
            g_send_function = dummy_send

        if max_parm is not None:
            global g_max_parm
            g_max_parm = max_parm

        if image_width is not None:
            global g_image_width
            if image_width > g_image_width:
                raise ValueError,(_('NgcGui image_width=%d too big, max=%d')
                                 % (image_width,g_image_width))
            g_image_width = image_width

        if g_max_parm > INTERP_SUB_PARAMS:
            raise ValueError,(_('max_parms=%d exceeds INTERP_SUB_PARAMS=%d')
                            %  (g_max_parm,INTERP_SUB_PARAMS) )

        ct_of_pages = 0
        try:
            import popupkeyboard
            import glib # for glib.GError
            if keyboardfile is not None:
                global g_popkbd
                if (keyboardfile in ('default','yes') ):
                    keyboardfile = g_keyboardfile
                g_popkbd = popupkeyboard.PopupKeyboard(glade_file=keyboardfile
                             ,use_coord_buttons=True
                             )
                global g_entry_height
                g_entry_height = g_big_height # bigger for popupkeyboard
        except ImportError, msg:
            print('\nImportError:\n%s', msg)
            print('keyboardfile=%s' % keyboardfile)
            print('popup keyboard unavailable\n')
        except glib.GError, msg:
            # can occur for toohigh version in ui file
            print('\nglib.GError:\n%s' % msg)
            print('keyboardfile=%s' % keyboardfile)
            print('popup keyboard unavailable\n')

        self.last_file = None
        self.nb = None
        self.autosend = not noauto
        self.expandsub = False
        self.nextpage_idx = 0
        self.startpage_idx = 0
        self.pg_for_npage = {}
        if w is None:
            # standalone operation
            self.nb = gtk.Notebook()
            w = gtk.Window(gtk.WINDOW_TOPLEVEL)
            if g_alive: w.connect("destroy", gtk.main_quit)
            w.set_title(sys.argv[0])
            w.add(self.nb)
            self.nb.show()
            w.show()
        elif type(w) == gtk.Frame:
            # demo -- embed as a notebook in a provider's frame
            self.nb = gtk.Notebook()
            w.add(self.nb)
            self.nb.show()
            w.show()
        elif type(w) == gtk.Notebook:
            # demo -- embed as additional pages in a provider's notebook
            self.nb = w
            self.startpage_idx = self.nb.get_n_pages()
        else:
            raise ValueError,'NgcGui:bogus w= %s' % type(w)

        self.nb.set_scrollable(True)
        self.set_theme(w,tname=gtk_theme_name)

        self.intfc = LinuxcncInterface(ini_file)

        if len(self.intfc.subroutine_path) == 0:
            self.intfc.addto_spath(
                       spath_from_files(pre_file,sub_files,pst_file))
            if len(self.intfc.subroutine_path) != 0:
                user_message(mtype=gtk.MESSAGE_WARNING
                    ,title=_('Simulated subroutine path')
                    ,msg=_('No subroutine path available.\n'
                      'Simulating subroutine path:\n\n')
                      + str(self.intfc.subroutine_path)
                      + '\n'
                      + _('Generated results may not be usable with linuxcnc')
                    )
        if len(self.intfc.subroutine_path) == 0:
            if g_alive:
                # no message if glade designer is running:
                user_message(mtype=gtk.MESSAGE_ERROR
                    ,title=_('No Subroutine Paths')
                    ,msg='\n' +
                        _('No paths available!\n'
                          'Make sure there is a valid\n'
                          '    [RS274]SUBROUTINE_PATH\n\n'
                          '     1) Start linuxcnc\n'
                          'or\n'
                          '     2) Specify an ini file\n'
                          'or\n'
                          '     3) Specify at least one subfile\n'
                          '\n')
                    )
                sys.exit(1)

        global g_searchpath; g_searchpath = self.intfc.subroutine_path


        # multiple pages can be specified with __init__()
        initsublist= []
        if type(sub_files) == StringType and sub_files:
            initsublist.append(sub_files)
        else:
            initsublist = sub_files

        nogo_l = []
        for sub_file in initsublist:
            if not g_alive: continue
            if os.path.dirname(sub_file) in self.intfc.subroutine_path:
                self.add_page(pre_file,sub_file,pst_file)
                ct_of_pages += 1
            else:
                nogo_l.append(sub_file)
        if nogo_l:
            user_message(mtype=gtk.MESSAGE_INFO
                    ,title=_('Cannot use files not in subroutine path')
                    ,msg=_('Files not in subroutine path:\n')
                         + str(nogo_l) +
                         '\n\n'
                         + _('Subroutine path is:\n')
                         + str(self.intfc.subroutine_path)
                    )

        nogo_l = []
        # multiple pages can be specified with an ini_file
        sublist  = self.intfc.get_subfiles()  #returns list
        pre_file = self.intfc.get_preamble()
        pst_file = self.intfc.get_postamble()

        # auto_file directory:
        # if specified, verify in path, give message if not
        # if nil
        #   if    PROGRAM_PREFIX  put there
        #   else                  put in cwd
        if auto_file:
            dir = os.path.abspath(os.path.dirname(auto_file))
            spath = self.intfc.get_subroutine_path()
            try:
                spath.index(dir) # check that auto_file dir is in path
                # auto_file ok
            except ValueError:
                # it's called autofile in --help
                pass
                #user_message(mtype=gtk.MESSAGE_WARNING
                #        ,title=_('Warning: autofile not in path')
                #        ,msg=_('autofile==%s is not in linuxcnc\n'
                #               'subroutine search path:\n'
                #               '  %s\n') % (auto_file,spath)
                #        )
            self.auto_file = auto_file
        else:
            pprefix = self.intfc.get_program_prefix()
            if pprefix:
                self.auto_file = os.path.join(pprefix,'auto.ngc')
            else:
                self.auto_file = os.path.join(os.path.curdir,'auto.ngc')

        dprint('input for auto_file=%s\nfinal auto_file=%s'
              % (auto_file,self.auto_file))

        if pre_file is None: pre_file  = ''
        if pst_file is None: pst_file = ''

#       vprint('SAVE_FILE: %s' % self.auto_file)
        if sublist and g_alive:
            for sub_file in sublist:
                if sub_file == '""': #beware code for custom is '""'
                    sub_file = ''
                try:
                    self.add_page(pre_file,sub_file,pst_file)
                    ct_of_pages += 1
                except Exception,detail:
                    exception_show(Exception,detail,src='NgcGui init')
                    print(_('CONTINUING without %s') % sub_file)
        else:
            if not sub_files:
                vprint('NgcGui: no ini_file with sublist '
                       'and no cmdline sub_file:'
                       'making Custom page')
                self.add_page('','','')
                ct_of_pages += 1
            pass

        self.current_page = None
        # self.nb.set_current_page(self.startpage_idx)
        # start at page 0 to respect caller's ordering
        self.nb.set_current_page(0)

        if g_alive: self.nb.connect('switch-page',   self.page_switched)
        w.show_all()

        if ct_of_pages == 0:
            usage()
            print(_('No valid subfiles specified'))
            sys.exit(1)
        return

    def update_fonts(self,fontname):
        update_fonts(fontname)

    def set_theme(self,w,tname=None):
        screen   = w.get_screen()
        settings = gtk.settings_get_for_screen(screen)
        if (tname is None) or (tname == "") or (tname == "Follow System Theme"):
            tname = settings.get_property("gtk-theme-name")
        settings.set_string_property('gtk-theme-name',tname,"")

    def page_switched(self,notebook,npage,pno):
        if self.current_page:
            curpage = self.current_page
            if hasattr(curpage,'imgw'):
                w = getattr(curpage,'imgw')
                w.iconify()
        try:
            mypg = self.pg_for_npage[self.nb.get_nth_page(pno)]
            if hasattr(mypg,'imgw'):
                w = getattr(mypg,'imgw')
                w.deiconify()
                w.show_all()
            self.current_page = mypg
        except KeyError,msg:
            # can occur when embedded in providers notebook
            # print('page_switched: Caught KeyError')
            pass

    def add_page(self,pre_file,sub_file,pst_file,imageoffpage=False):
        # look for gcmc on first request for .gcmc file:
        if os.path.splitext(sub_file)[-1] in ['.gcmc','.GCMC']:
            if not find_gcmc(): return None

        self.nextpage_idx = self.nextpage_idx + 1
        opage = OnePg(pre_file=pre_file
                     ,sub_file=sub_file
                     ,pst_file=pst_file
                     ,mynb=self.nb
                     ,nset=self # an NgcGui set of pages
                     ,imageoffpage=imageoffpage
                     )
        if opage.fset.sub_data.pdict['subname'] == '':
            ltxt = 'Custom'
        else:
            ltxt = opage.fset.sub_data.pdict['subname']
        ltxt = self.make_unique_tab_name(ltxt)

        eb_lbl = gtk.EventBox()
        mylbl  = gtk.Label(ltxt)
        if g_popkbd is not None:
             mylbl.set_size_request(-1,g_big_height)
        eb_lbl.add(mylbl)
        mylbl.show()
        eb_lbl.set_style(g_lbl_style_default)

        pno  = self.nb.append_page(opage.pgbox,eb_lbl)
        if g_control_font is not None:
            mod_font_by_category(mylbl)

        # An EventBox is needed to change bg of tabpage label
        # When using EventBox:
        #      don't use get_tab_label_text()
        opage.save_onepage_tablabel(eb_lbl,mylbl)

        self.pg_for_npage[self.nb.get_nth_page(pno)] = opage
        self.nb.set_current_page(pno) # move to the new page
        return opage

    def make_unique_tab_name(self,name):
        l = []
        if not name: return None
        for pno in range(self.startpage_idx,self.nb.get_n_pages()):
            npage = self.nb.get_nth_page(pno)
            pg = self.pg_for_npage[npage]
            # using EventBox for label, dont use get_tab_label_text()
            ltxt = pg.the_lbl.get_text()
            if ltxt.find(name) == 0:
                l.append(ltxt)
        if len(l) == 0:
            return(name)
        if len(l) == 1:
            return(name + '-1')
        last = l[-1]
        idx = last.find('-')
        return(name + '-' + str(int(last[idx+1:]) + 1) )


class SaveSection():
    """SaveSection: lines ready for result file"""
    def __init__(self,mypg,pre_info,sub_info,pst_info,force_expand=False):
        global g_label_id
        g_label_id += 1
        self.sdata=[]

        self.sdata.append("(%s: FEATURE %s)\n"% (g_progname,dt() ))

        self.sdata.append("(%s: files: <%s,%s,%s>)\n"
                       % (g_progname
                         ,pre_info.pre_file
                         ,sub_info.sub_file
                         ,pst_info.pst_file
                         )
                       )

        # note: this line will be replaced on file output with a count
        # that can span multiple pages
        self.sdata.append("#<_feature:> = 0\n")

        self.sdata.append("(%s: preamble file: %s)\n" % (
                          g_progname,pre_info.pre_file))
        self.sdata.extend(pre_info.inputlines)

        emsg = '' # accumulate errors for emsg

        calltxt = 'o<%s> call ' % sub_info.pdict['subname']
        parmlist = []
        tmpsdata = []
        for idx in sub_info.ndict:
            name,dvalue,comment = sub_info.ndict[idx]
            value=mypg.efields.getentry_byidx(idx)
            try:
                v = float(value)
            except ValueError:
                emsg = emsg + (
                     _('Entry for parm %2d is not a number\n <%s>\n')
                     % (int(idx),value))
            #note: e formats not accepted by linuxcnc (like 1e2)
            #      but using float(value) --->mmm.nnnnn everywhere
            #      makes long call line
            #      so try to send entry value, but if it has e, use float
            if 'e' in value:
                value = str(float(value.lower() ))

            parmlist.append(value)
            if sub_info.pdict.has_key('isgcmc'):
                # just print value of gcmc parm embedded in gcmc result
                # the call requires no parms
                pass
            else:
                calltxt = calltxt + '[%s]' % value
            # these appear only for not-expandsub
            tmpsdata.append("(%11s = %12s = %12s)\n" % (
                              '#'+str(idx),name,value))
        if emsg:
            user_message(mtype=gtk.MESSAGE_ERROR
                        ,title=_('SaveSection Error')
                        ,msg=emsg)
            mypg.cpanel.set_message(_('Failed to create feature'))
            raise ValueError
        calltxt = calltxt + '\n'
        # expandsub not honored for gcmc
        if (mypg.expandsub and sub_info.pdict.has_key('isgcmc')):
            print(_('expandsub not honored for gcmc file: %s')%
                     os.path.basename(sub_info.sub_file))
            mypg.expandsub = 0
        #---------------------------------------------------------------------
        if (not mypg.expandsub) and (not force_expand):
            self.sdata.append("(%s: call subroutine file: %s)\n" % (
                              g_progname,sub_info.sub_file) )
            self.sdata.append("(%s: positional parameters:)\n"% g_progname)
            self.sdata.extend(tmpsdata)
            self.sdata.append(calltxt) # call the subroutine
        else:
            # expand the subroutine in place with unique labels
            self.sdata.append('(Positional parameters for %s)\n'
                             % mypg.sub_file)
            for i in range(0,idx):
                self.sdata.append('        #%d = %s\n' % (i+1,parmlist[i]))
            self.sdata.append('(expanded file: %s)\n' % mypg.sub_file)
            blank = ''
            idx = 0
            for line in sub_info.inputlines:
                idx += 1
                if line.strip() == '':
                    continue
                if idx in sub_info.ldict:
                    modlabel = sub_info.ldict[idx]
                    if modlabel == 'ignoreme':
                        continue
                    modlabel = 'o<%03d%s>' % (g_label_id,modlabel)
                    r = re.search(r'^o<(.*?)>(.*)',line)
                    if r:
                        modline = r.group(2) + '\n'
                    else:
                        print('SaveSection__init__:unexpected:',line)
                    self.sdata.append('%11s %s' % (modlabel,modline))
                else:
                    theline = '%11s %s' % (blank,line)
                    # hack: try to reduce long line length so linuxcnc wont
                    #       choke on files that work otherwise but fail
                    #       when expanded here
                    # example: 246 chars observed for
                    #  qpex --> the call to qpocket uses many named parms
                    # hardcoded for # config.h.in #define LINELEN 255
                    # hardcoded 252 empiracally determined
                    if len(theline) >= 252:
                        theline = line
                    self.sdata.append(theline)
        #---------------------------------------------------------------------

        if pst_info.inputlines:
            self.sdata.append("(%s: postamble file: %s)\n" % (
                              g_progname,pst_info.pst_file))
            self.sdata.extend(pst_info.inputlines)
        #for line in self.sdata:
        #    print('line:',line,)


def usage():
    print("""
Usage:
%s [Options] [sub_filename]
Options requiring values:
    [-d | --demo] [0|1|2] (0: DEMO standalone toplevel)
                          (1: DEMO embed new notebook)
                          (2: DEMO embed within existing notebook)
    [-S | --subfile       sub_filename]
    [-p | --preamble      preamble_filename]
    [-P | --postamble     postamble_filename]
    [-i | --ini           inifile_name]
    [-a | --autofile      auto_filename]
    [-t | --test          testno]
    [-K | --keyboardfile  glade_file] (use custom popupkeyboard glade file)
Solo Options:
    [-v | --verbose]
    [-D | --debug]
    [-N | --nom2]         (no m2 terminator (use %%))
    [-n | --noauto]       (save but do not automatically send result)
    [-k | --keyboard]     (use default popupkeybaord)
    [-s | --sendtoaxis]   (send generated ngc file to axis gui)
Notes:
      A set of files is comprised of a preamble, subfile, postamble.
      The preamble and postamble are optional.
      One set of files can be specified from cmdline.
      Multiple sets of files can be specified from an inifile.
      If --ini is NOT specified:
         search for a running linuxcnc and use its inifile
    """ % g_progname)
#-----------------------------------------------------------------------------
# Standalone (and demo) usage:

def standalone_pyngcgui():
    # make widgets for test cases:
    top = gtk.Window(gtk.WINDOW_TOPLEVEL)
    top.set_title('top')
    hbox  = gtk.HBox()
    top.add(hbox)
    l1 = gtk.Label('LABEL')
    hbox.pack_start(l1,expand=0,fill=0)
    e1 = gtk.Entry()
    hbox.pack_start(e1,expand=0,fill=0)
    e1.set_width_chars(4)
    f1 = gtk.Frame()
    hbox.pack_start(f1,expand=0,fill=0)
    f2 = gtk.Frame()
    hbox.pack_start(f2,expand=0,fill=0)

    n = gtk.Notebook()
    n.set_scrollable(True)
    b1 = gtk.Button('b1-filler')
    b2 = gtk.Button('b2-filler')
    n.append_page(b1,gtk.Label('Mb1-filler'))
    n.append_page(b2,gtk.Label('Mb2-filler'))
    f1.add(n)
    top.show_all()


    demo         = 0 # 0 ==> standalone operation
    subfilenames = ''
    prefilename  = ''
    pstfilename  = ''
    vbose        = False
    dbg          = False
    noauto       = False
    keyboard     = False
    keyboardfile = 'default'
    ini_file     = ''
    auto_file    = ''
    tmode        = 0
    send_f       = default_send
    try:
        options,remainder = getopt.getopt(sys.argv[1:]
                          ,'a:Dd:hi:kK:Nnp:P:sS:t:v'
                          , ['autofile'
                            ,'demo='
                            ,'debug'
                            ,'help'
                            ,'ini='
                            ,'keyboard'
                            ,'keyboardfile='
                            ,'noauto'
                            ,'preamble='
                            ,'postamble='
                            ,'subfile='
                            ,'verbose'
                            ,'sendtoaxis'
                            ,'nom2'
                            ]
                          )
    except getopt.GetoptError,msg:
        usage()
        print('\nGetoptError:%s' % msg)
        sys.exit(1)
    except Exception, detail:
        exception_show(Exception,detail,'__main__')
        sys.exit(1)
    for opt,arg in options:
        #print('#opt=%s arg=%s' % (opt,arg))
        if opt in ('-h','--help'):      usage(),sys.exit(0)
        if opt in ('-d','--demo'):      demo = arg


        if opt in ('-i','--ini'):       ini_file = arg
        if opt in ('-a','--autofile'):  auto_file = arg

        if opt in ('-p','--preamble'):  prefilename=arg
        if opt in ('-P','--postamble'): pstfilename=arg
        if opt in ('-S','--subfile'):   subfilenames=arg

        if opt in ('-t','--test'):      tmode=arg


        if opt in ('-k','--keyboard'):   keyboard=True
        if opt in ('-K','--keyboardfile'):
            keyboard=True
            keyboardfile=arg

        if opt in ('-N','--nom2'):       dbg = g_nom2 = True
        if opt in ('-D','--debug'):      dbg = True
        if opt in ('-n','--noauto'):     noauto = True
        if opt in ('-v','--verbose'):
            vbose = True
            continue
        if opt in ('-s','--sendtoaxis'):
            send_f = send_to_axis
            continue
    if remainder: subfilenames = remainder # ok for shell glob e.g., *.ngc
    demo = int(demo)
    if not keyboard: keyboardfile=None

    if (dbg):
        print(g_progname + ' BEGIN-----------------------------------------------')
        print('    __file__= %s' % __file__)
        print('    ini_file= %s' % ini_file)
        print('    sys.argv= %s' % sys.argv)
        print('   os.getcwd= %s' % os.getcwd())
        print('    sys.path= %s' % sys.path)
        print('        demo= %s' % demo)
        print(' prefilename= %s' % prefilename)
        print('subfilenames= %s' % subfilenames)
        print(' pstfilename= %s' % pstfilename)
        print('    keyboard= %s, keyboardfile= <%s>' % (keyboard,keyboardfile))
    try:
        if demo == 0:
            top.hide()
            NgcGui(w=None
                  ,verbose=vbose,debug=dbg,noauto=noauto
                  ,keyboardfile=keyboardfile
                  ,tmode=tmode
                  ,send_function=send_f # prototype: (fname)
                  ,ini_file=ini_file,auto_file=auto_file
                  ,pre_file=prefilename,sub_files=subfilenames,pst_file=pstfilename
                  )
        elif demo == 1:
            NgcGui(w=f2
                  ,verbose=vbose,debug=dbg,noauto=noauto
                  ,keyboardfile=keyboardfile
                  ,tmode=tmode
                  ,send_function=send_f # prototype: (fname)
                  ,ini_file=ini_file,auto_file=auto_file
                  ,pre_file=prefilename,sub_files=subfilenames,pst_file=pstfilename
                  )
            top.set_title('Create OnePg inside a new frame')
        elif demo == 2:
            NgcGui(w=n
                  ,verbose=vbose,debug=dbg,noauto=noauto
                  ,keyboardfile=keyboardfile
                  ,tmode=tmode
                  ,send_function=send_f # prototype: (fname)
                  ,ini_file=ini_file,auto_file=auto_file
                  ,pre_file=prefilename,sub_files=subfilenames,pst_file=pstfilename
                  )
            top.set_title('Create OnePg inside an existing notebook')
        else:
            print('unknown demo',demo)
            usage()
            sys.exit(1)
    except Exception, detail:
        exception_show(Exception,detail,'__main__')
        print('in main()')
        sys.exit(11)

    try:
        gtk.main()
    except KeyboardInterrupt:
        sys.exit(0)

# vim: sts=4 sw=4 et
