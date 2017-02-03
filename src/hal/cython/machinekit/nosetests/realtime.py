import subprocess
import os
DEBUG="5"

def setup_module():
    e=os.environ.copy()
    e["DEBUG"] = DEBUG
    subprocess.call("realtime restart", shell=True,stderr=subprocess.STDOUT,env=e)

def teardown_module():
    e=os.environ.copy()
    e["DEBUG"] = DEBUG
    subprocess.call("realtime stop", shell=True,stderr=subprocess.STDOUT,env=e)
