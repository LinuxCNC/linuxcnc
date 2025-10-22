import os
import sys

# Set up logging
from . import logger

LOG = logger.getLogger(__name__)


# Force the log level for this module
# LOG.setLevel(logger.VERBOSE) # One of VERBOSE, DEBUG, INFO, WARNING, ERROR, CRITICAL

def search(filename, postfix):
        # widget directory
        here = os.path.dirname(os.path.realpath(__file__))

        RIP_FLAG = bool(os.environ.get('LINUXCNC_RIP_FLAG', False))

        LOG.debug(f'RIP environmental variable flag: {RIP_FLAG}')

        if RIP_FLAG:
            HOMEBATH = os.environ.get('EMC2_HOME', None)
        else:
            HOMEBATH = os.environ.get('LINUXCNC_HOME', None)
            # fallback until the RIP_FLAG is common
            if HOMEBATH is None:
               HOMEBATH = os.environ.get('EMC2_HOME', None)

        LOG.debug('Linuxcnc Home directory found in environmental variable: {}'.format(HOMEBATH))

        PANELDIR = os.path.join(here,'builtin-panels')
        BASENAME = os.path.splitext(os.path.basename(filename))[0]
        # base path (includes any extra path commands
        BASEPATH = os.path.splitext(filename)[0]
        LOG.debug(f'Passed name={filename}, BASENAME={BASENAME} BASEPATH={BASEPATH}')

        # look for custom ui file
        WORKINGDIR = os.getcwd()
        LOG.verbose('Working directory: {}'.format(WORKINGDIR))

        if postfix == 1:
            fn = "{}.glade".format(BASEPATH)
            name = '.glade'
        elif postfix == 2:
            fn = "{}.py".format(BASEPATH)
            name = '.py'

        local = []
        local.append( os.path.join(WORKINGDIR,'gladevcp/builtin-panels' ,BASEPATH, fn))
        local.append( os.path.join(WORKINGDIR, fn))
        defaultui = os.path.join(PANELDIR, BASEPATH, fn)
        for localfn in local:
            LOG.debug("Checking for {} in: yellow<{}>".format(name, localfn))
            if os.path.exists(localfn):
                LOG.info("bggreen<Using LOCAL {} file from:> yellow<{}>".format(name, localfn))
                XML = localfn
                break
        # if no break
        else:
            LOG.debug("Checking for {} in: yellow<{}>".format(name,defaultui))
            if os.path.exists(defaultui):
                LOG.info("bggreen<Using DEFAULT {} file from:> yellow<{}>".format(name, defaultui))
                XML = defaultui
            else:
                # error
                XML = None
                LOG.info(f"No {name} file found.")
        return XML

