# coding=utf-8
import os
from six.moves import configparser
from machinekit import rtapi, hal

# retrieve the machinekit UUID
cfg = configparser.ConfigParser()
cfg.read(os.getenv("MACHINEKIT_INI"))
uuid = cfg.get("MACHINEKIT", "MKUUID")

# open a command channel to rtapi_app
rt = rtapi.RTAPIcommand(uuid=uuid)
