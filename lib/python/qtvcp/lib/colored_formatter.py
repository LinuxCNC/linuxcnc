#!/usr/bin/env python

#   Copyright (c) 2017 Kurt Jacobson

#  Permission is hereby granted, free of charge, to any person obtaining a copy
#  of this software and associated documentation files (the "Software"), to deal
#  in the Software without restriction, including without limitation the rights
#  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#  copies of the Software, and to permit persons to whom the Software is
#  furnished to do so, subject to the following conditions:

#  The above copyright notice and this permission notice shall be included in all
#  copies or substantial portions of the Software.

#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#  SOFTWARE.

import re
import time
from copy import copy
from logging import Formatter

PREFIX = '\033['
SUFFIX = '\033[0m'

COLORS = {
    'black'  : 30,
    'red'    : 31,
    'green'  : 32,
    'yellow' : 33,
    'blue'   : 34,
    'magenta': 35,
    'cyan'   : 36,
    'white'  : 37,
    'bgred'  : 41,
    'bggrey' : 100
    }

MAPPING = {
    'DEBUG'   : 'white',
    'INFO'    : 'cyan',
    'WARNING' : 'yellow',
    'ERROR'   : 'red',
    'CRITICAL': 'bgred',
}


# Returns `text` warped in ASCII escape codes to produce `color`
def COLORIZE(text, color=None):
    seq = COLORS.get(color, 37) # default to white
    return '{0}{1}m{2}{3}'.format(PREFIX, seq, text, SUFFIX)


# Matches only the first `color<text>` occurrence
# ^(.*?)<([^)]+)> 

# Matches all `color<text>` occurrences, both take the same number of steps
# ([^<\s]+)<([^>]+)>
# (\w+)<([^>]+)>

RE = re.compile(r'(\w+)<([^>]+)>')


class ColoredFormatter(Formatter):

    def __init__(self, patern):
        Formatter.__init__(self, patern)

    # Override the Formatter's format method to add ASCII colors
    # to the levelname and any marked words in the log message.
    def format(self, record):
        colored_record = copy(record)

        # Add colors to levelname
        levelname = colored_record.levelname
        color = MAPPING.get(levelname, 'white')
        colored_record.levelname = COLORIZE(levelname, color)

        # Add colors to tagged message text
        msg = colored_record.getMessage()
        plain_msg, color_msg = self.color_words(msg)
        record.msg = plain_msg
        colored_record.msg = color_msg

        return Formatter.format(self, colored_record)

    # Replace `color<message>` in the log message with ASCII codes to colorize
    # the word or phrase in the terminal log handler.  Also return a cleaned
    # version of the message with the tags removed for use by the file handler.
    def color_words(self, raw_msg):
        plain_msg = color_msg = raw_msg
        if '<' in raw_msg: # If no tag don't try to match
            iterater = RE.finditer(raw_msg)
            if iterater:
                for match in iterater:
                    group = match.group()
                    color = match.group(1)
                    word = match.group(2)

                    # Could be optimized, but will rarely have more than one match
                    color_msg = color_msg.replace(group, COLORIZE(word, color))
                    plain_msg = plain_msg.replace(group, word)

        return plain_msg, color_msg



# ----------------------- E X A M P L E -----------------------

if __name__ == '__main__':

    import logging

    # Create logger
    log = logging.getLogger(__name__)
    log.setLevel(logging.DEBUG)

    # Add console handler
    ch = logging.StreamHandler()
    ch.setLevel(logging.DEBUG)
    cf = ColoredFormatter("[%(name)s][%(levelname)s]  %(message)s (%(filename)s:%(lineno)d)")
    ch.setFormatter(cf)
    log.addHandler(ch)

    # Add file handler
    fh = logging.FileHandler('demo.log')
    fh.setLevel(logging.DEBUG)
    ff = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    fh.setFormatter(ff)
    log.addHandler(fh)

    # Log some stuff
    log.debug("Logging demo has green<STARTED>")
    log.info("Logging to yellow<demo.log> in the script dir")
    log.warning("This is my last warning, red<take heed>")
    log.error("This is an error")
    log.critical("He's dead, Jim")

    # Example exception logging
    try:
        print False + "True"
    except Exception as e:
        log.debug('That did not work!', exc_info=e)

