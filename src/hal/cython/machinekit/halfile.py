import os
import ConfigParser
from machinekit import rtapi,hal,config

# retrieve the machinekit UUID
cfg = ConfigParser.ConfigParser()
mkini = os.getenv("MACHINEKIT_INI")
if mkini is None:
    mkini = config.Config().MACHINEKIT_INI
cfg.read(mkini)
uuid = cfg.get("MACHINEKIT", "MKUUID")

# open a command channel to rtapi_app
rt = rtapi.RTAPIcommand(uuid=uuid)
