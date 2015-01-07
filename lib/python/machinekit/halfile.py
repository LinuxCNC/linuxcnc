import os
import ConfigParser
from machinekit import rtapi,hal

# retrieve the machinekit UUID
cfg = ConfigParser.ConfigParser()
cfg.read(os.getenv("MACHINEKIT_INI"))
uuid = cfg.get("MACHINEKIT", "MKUUID")

# open a command channel to rtapi_app
rt = rtapi.RTAPIcommand(uuid=uuid)
