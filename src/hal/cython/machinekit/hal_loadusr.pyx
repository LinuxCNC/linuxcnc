# the loadusr function
import subprocess
import shlex
import time

def loadusr(command, wait=False, wait_name=None, wait_timeout=5.0, shell=False):
    cmd = command
    if not shell:
        cmd = shlex.split(command)  # correctly split the command
    p = subprocess.Popen(cmd, shell=shell)

    wait = wait or (wait_name is not None)
    if not wait:
        return

    timeout = 0.0
    while True:
        p.poll()
        # check if process is alive
        ret = p.returncode
        if ret is not None and ret != 0:
            raise RuntimeError(command + ' exited with return code ' + str(ret))
        # check if component exists
        if (wait_name is not None) and (wait_name in components):
            return
        # check for timeout
        if timeout >= wait_timeout:
            p.kill()
            raise RuntimeError(command + ' did not start')

        timeout += 0.1
        time.sleep(0.1)
