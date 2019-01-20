from qtvcp.core import Status, Info

import os
import time

# Set up logging
from qtvcp import logger
log = logger.getLogger(__name__)
# Set the log level for this module
# log.setLevel(logger.INFO) # One of DEBUG, INFO, WARNING, ERROR, CRITICAL

STATUS = Status()
INFO = Info()


class MachineLogger():
    def __init__(self):
        STATUS.connect('update-machine-log', self.log_it)
        self.mlp = os.path.expanduser(INFO.MACHINE_LOG_HISTORY_PATH)

    def log_it(self, w, message, option):
        if option == 'TIME':
            self.log_message_time(message)
        elif option == 'DATE':
            self.log_message_date(message)
        elif option == 'DELETE':
            self.delete_log()
        elif option == 'INITIAL':
            self.initial_greeting()
        else:
            self.log_message(message)

    def initial_greeting(self):
        try:
            timestamp = time.strftime("%a, %b %d %Y %X ---\n")
            fp = open(self.mlp, 'a')

            #fp.write(""" $$$$$$\  $$$$$$$$\ """)
            #fp.write('\n')
            #fp.write('''$$  __$$\ \__$$  __|''')
            #fp.write('\n')
            #fp.write('$$ /  $$ |   $$ |  $$$$$$$\  $$$$$$$\  $$$$$$\   $$$$$$\   $$$$$$\  $$$$$$$\  ')
            #fp.write('\n')
            #fp.write('$$ |  $$ |   $$ | $$  _____|$$  _____|$$  __$$\ $$  __$$\ $$  __$$\ $$  __$$\ ')
            #fp.write('\n')
            #fp.write('$$ |  $$ |   $$ | \$$$$$$\  $$ /      $$ |  \__|$$$$$$$$ |$$$$$$$$ |$$ |  $$ |')
            #fp.write('\n')
            #fp.write('$$ $$\$$ |   $$ |  \____$$\ $$ |      $$ |      $$   ____|$$   ____|$$ |  $$ |')
            #fp.write('\n')
            #fp.write('\$$$$$$ /    $$ | $$$$$$$  |\$$$$$$$\ $$ |      \$$$$$$$\ \$$$$$$$\ $$ |  $$ |')
            #fp.write('\n')
            #fp.write(' \___$$$\    \__| \_______/  \_______|\__|       \_______| \_______|\__|  \__|')
            #fp.write('\n')
            #fp.write('     \___|        ')

            fp.write('--- Qtvcp Screen Started on: ' + timestamp)
            fp.close()
        except:
            log.warning('machine log history: path valid?')

    def log_message_time(self, message):
        try:
            timestamp = time.strftime("%a%d %H:%M ")
            fp = open(self.mlp, 'a')
            fp.write(timestamp + message + "\n")
            fp.close()
        except:
            log.warning('machine log history: path valid?: {}'.format(fp))
        STATUS.emit('machine-log-changed')

    def log_message_date(self, message):
        try:
            timestamp = time.strftime("%a, %b %d %Y %X ---\n")
            fp = open(self.mlp, 'a')
            fp.write(timestamp + message)
            fp.close()
        except:
            log.warning('machine log history: path valid?')
        STATUS.emit('machine-log-changed')

    def log_message(self, message):
        try:
            fp = open(self.mlp, 'a')
            fp.write(message)
            fp.close()
        except:
            log.warning('machine log history: path valid?')
        STATUS.emit('machine-log-changed')

    def delete_log(self):
        fp = open(self.mlp, 'w')
        fp.write('')
        fp.close()
        STATUS.emit('machine-log-changed')


