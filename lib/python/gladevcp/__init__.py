from hal_pythonplugin import *

try:
    import importlib
    support_aux_apps = True
except:
    support_aux_apps = False
    print "\n"
    print "Gladevcp: Cannot import importlib"
    print "          Auxiliary Gladevcp apps not available\n"

#----------------------------------------------------------------------
# Support auxiliary gladevcp apps
import os,sys,glob,subprocess

def excluded_filename(fname):
    exclude_list = ["install","setup"]
    for e in exclude_list:
        if e in fname: return True
    return False

if support_aux_apps:
    #----------------------------------------------------------------------
    modnames = []
    #----------------------------------------------------------------------
    # Auxiliary gladevcp apps specified by environmental variable
    gladevcp_user_extras = os.getenv('GLADEVCP_EXTRAS')
    if gladevcp_user_extras is not None:
        print "gladevcp: GLADEVCP_EXTRAS:",gladevcp_user_extras
        for extradir in gladevcp_user_extras.split(":"):
            for fname in glob.glob(extradir + "/*.py"):
                if excluded_filename(fname):
                    print "gladevcp: excluded filename:",fname
                    continue
                modname = os.path.basename(fname).split(".")[0]
                if modname in modnames:
                    print "gladevcp: rejecting duplicate:",fname
                    continue
                modnames.append(modname)
                sys.path.insert(0,extradir) # prepend
                importlib.import_module(modname)
                print "gladevcp: importing:",fname
    #----------------------------------------------------------------------
    # Auxiliary gladevcp apps may be installed in a known location
    # location defined by the substitution item LINUXCNC_AUX_GLADEVCP.
    # The location is available from the script linuxcnc_var
    # (this script should always be in PATH for both RIP builds and
    # deb installs of LinuxCNC)
    s = subprocess.Popen(['linuxcnc_var','LINUXCNC_AUX_GLADEVCP']
                        ,stdout=subprocess.PIPE
                        ,stderr=subprocess.PIPE
                        )
    p,e = s.communicate()
    gladevcp_aux_apps_dir = p.strip() # remove trailing \n
    for auxdir in glob.glob(gladevcp_aux_apps_dir + "/*"):
        print "gladevcp: auxiliary dir:",auxdir
        for fname in glob.glob(auxdir + "/*.py"):
            if excluded_filename(fname):
                print "gladevcp: excluded filename:",fname
                continue
            modname = os.path.basename(fname).split(".")[0]
            if modname in modnames:
                print "gladevcp: rejecting duplicate:",fname
                continue
            modnames.append(modname)
            sys.path.insert(0,auxdir) # prepend
            importlib.import_module(modname)
            print "gladevcp: importing:",fname
    
    #----------------------------------------------------------------------
