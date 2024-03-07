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

    def log_it(self, w, message, option=None):
        if option == 'DELETE':
            self.delete_log()
            return
        try:
            message = message.rstrip('\n')
            if option == 'TIME':
                self.log_message_time(message)
            elif option == 'DATE':
                self.log_message_date(message)
            elif option == 'INITIAL':
                self.initial_greeting()
            else:
                self.log_message(message)
        except Exception as e:
            log.exception('log_it function: {}'.format(e))

    def initial_greeting(self):
        try:
            timestamp = time.strftime("%a, %b %d %Y %X ---")
            fp = open(self.mlp, 'a')

            # fp.write(""" $$$$$$\  $$$$$$$$\ """)
            # fp.write('\n')
            # fp.write('''$$  __$$\ \__$$  __|''')
            # fp.write('\n')
            # fp.write('$$ /  $$ |   $$ |  $$$$$$$\  $$$$$$$\  $$$$$$\   $$$$$$\   $$$$$$\  $$$$$$$\  ')
            # fp.write('\n')
            # fp.write('$$ |  $$ |   $$ | $$  _____|$$  _____|$$  __$$\ $$  __$$\ $$  __$$\ $$  __$$\ ')
            # fp.write('\n')
            # fp.write('$$ |  $$ |   $$ | \$$$$$$\  $$ /      $$ |  \__|$$$$$$$$ |$$$$$$$$ |$$ |  $$ |')
            # fp.write('\n')
            # fp.write('$$ $$\$$ |   $$ |  \____$$\ $$ |      $$ |      $$   ____|$$   ____|$$ |  $$ |')
            # fp.write('\n')
            # fp.write('\$$$$$$ /    $$ | $$$$$$$  |\$$$$$$$\ $$ |      \$$$$$$$\ \$$$$$$$\ $$ |  $$ |')
            # fp.write('\n')
            # fp.write(' \___$$$\    \__| \_______/  \_______|\__|       \_______| \_______|\__|  \__|')
            # fp.write('\n')
            # fp.write('     \___|        ')

            fp.write('--- QtVCP Screen Started on: ' + timestamp + "\n")
            fp.close()
        except:
            log.warning('machine log history: path valid?')

    def log_message_time(self, message):
        try:
            timestamp = time.strftime("%a%d %H:%M: ")
            fp = open(self.mlp, 'a')
            for num,i in enumerate(message.split('\\n')):
                if num == 0:
                    fp.write(timestamp + i + "\n")
                else:
                    fp.write(i + "\n")
            fp.close()
        except:
            log.warning('machine log history: path valid?: {}'.format(fp))
        STATUS.emit('machine-log-changed')

    def log_message_date(self, message):
        try:
            timestamp = time.strftime("%a, %b %d %Y %X: ")
            fp = open(self.mlp, 'a')
            for num,i in enumerate(message.split('\\n')):
                if num == 0:
                    fp.write(timestamp + i + "\n")
                else:
                    fp.write(i + "\n")
            fp.close()
        except:
            log.warning('machine log history: path valid?')
        STATUS.emit('machine-log-changed')

    def log_message(self, message):
        try:
            fp = open(self.mlp, 'a')
            for i in message.split('\\n'):
                    fp.write(i + "\n")
            fp.close()
        except:
            log.warning('machine log history: path valid?')
        STATUS.emit('machine-log-changed')

    def delete_log(self):
        fp = open(self.mlp, 'w')
        fp.write('')
        fp.close()
        STATUS.emit('machine-log-changed')
