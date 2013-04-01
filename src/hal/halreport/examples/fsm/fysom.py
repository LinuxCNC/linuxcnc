#
# fysom.py - pYthOn Finite State Machine - this is a port of Jake
#            Gordon's javascript-state-machine to python
#            https://github.com/jakesgordon/javascript-state-machine
#
# Copyright (C) 2011 Mansour Behabadi <mansour@oxplot.com>, Jake Gordon
#                    and other contributors
#
# Permission is hereby granted, free of charge, to any person obtaining
# a copy of this software and associated documentation files (the
# "Software"), to deal in the Software without restriction, including
# without limitation the rights to use, copy, modify, merge, publish,
# distribute, sublicense, and/or sell copies of the Software, and to
# permit persons to whom the Software is furnished to do so, subject to
# the following conditions:
#
# The above copyright notice and this permission notice shall be
# included in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
# IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
# CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
# TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
# SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
#

"""
USAGE

from fysom import Fysom

fsm = Fysom({
  'initial': 'green',
  'events': [
    {'name': 'warn',  'src': 'green',  'dst': 'yellow'},
    {'name': 'panic', 'src': 'yellow', 'dst': 'red'},
    {'name': 'calm',  'src': 'red',    'dst': 'yellow'},
    {'name': 'clear', 'src': 'yellow', 'dst': 'green'}
  ]
})

... will create an object with a method for each event:

  - fsm.warn()  - transition from 'green'  to 'yellow'
  - fsm.panic() - transition from 'yellow' to 'red'
  - fsm.calm()  - transition from 'red'    to 'yellow'
  - fsm.clear() - transition from 'yellow' to 'green'

along with the following members:

  - fsm.current    - contains the current state
  - fsm.isstate(s) - return True if state s is the current state
  - fsm.can(e)     - return True if event e can be fired in the current
                     state
  - fsm.cannot(e)  - return True if event s cannot be fired in the
                     current state

MULTIPLE SRC AND TO STATES FOR A SINGLE EVENT

fsm = Fysom({
  'initial': 'hungry',
  'events': [
    {'name': 'eat',  'src': 'hungry',    'dst': 'satisfied'},
    {'name': 'eat',  'src': 'satisfied', 'dst': 'full'},
    {'name': 'eat',  'src': 'full',      'dst': 'sick'},
    {'name': 'rest', 'src': ['hungry', 'satisfied', 'full', 'sick'],
                                         'dst': 'hungry'}
  ]
})

This example will create an object with 2 event methods:

  - fsm.eat()
  - fsm.rest()

The rest event will always transition to the hungry state, while the eat
event will transition to a state that is dependent on the current state.

NOTE the rest event in the above example can also be specified as
multiple events with the same name if you prefer the verbose approach.

CALLBACKS

4 callbacks are available if your state machine has methods using the
following naming conventions:

  - onbefore_event_ - fired before the _event_
  - onleave_state_  - fired when leaving the old _state_
  - onenter_state_  - fired when entering the new _state_
  - onafter_event_  - fired after the _event_

You can affect the event in 2 ways:

  - return False from an onbefore_event_ handler to cancel the event.
  - return False from an onleave_state_ handler to perform an
    asynchronous state transition (see next section)

For convenience, the 2 most useful callbacks can be shortened:

  - on_event_ - convenience shorthand for onafter_event_
  - on_state_ - convenience shorthand for onenter_state_

In addition, a generic onchangestate() calback can be used to call a
single function for all state changes.

All callbacks will be passed one argument 'e' which is an object with
following attributes:

  - fsm   Fysom object calling the callback
  - event Event name
  - src   Source state
  - dst   Destination state
  - (any other keyword arguments you passed into the original event
     method)

Note that when you call an event, only one instance of 'e' argument is
created and passed to all 4 callbacks. This allows you to preserve data
across a state transition by storing it in 'e'. It also allows you to
shoot yourself in the foot if you're not careful.

Callbacks can be specified when the state machine is first created:

def onpanic(e): print 'panic! ' + e.msg
def oncalm(e): print 'thanks to ' + e.msg
def ongreen(e): print 'green'
def onyellow(e): print 'yellow'
def onred(e): print 'red'

fsm = Fysom({
  'initial': 'green',
  'events': [
    {'name': 'warn',  'src': 'green',  'dst': 'yellow'},
    {'name': 'panic', 'src': 'yellow', 'dst': 'red'},
    {'name': 'panic', 'src': 'green',  'dst': 'red'},
    {'name': 'calm',  'src': 'red',    'dst': 'yellow'},
    {'name': 'clear', 'src': 'yellow', 'dst': 'green'}
  ],
  'callbacks': {
    'onpanic':  onpanic,
    'oncalm':   oncalm,
    'ongreen':  ongreen,
    'onyellow': onyellow,
    'onred':    onred
  }
})

fsm.panic(msg='killer bees')
fsm.calm(msg='sedatives in the honey pots')

Additionally, they can be added and removed from the state machine at
any time:

def printstatechange(e):
  print 'event: %s, src: %s, dst: %s' % (e.event, e.src, e.dst)

del fsm.ongreen
del fsm.onyellow
del fsm.onred
fsm.onchangestate = printstatechange

ASYNCHRONOUS STATE TRANSITIONS

Sometimes, you need to execute some asynchronous code during a state
transition and ensure the new state is not entered until you code has
completed.

A good example of this is when you run a background thread to download
something as result of an event. You only want to transition into the
new state after the download is complete.

You can return False from your onleave_state_ handler and the state
machine will be put on hold until you are ready to trigger the
transition using transition() method.

Example: TODO

INITIALIZATION OPTIONS

How the state machine should initialize can depend on your application
requirements, so the library provides a number of simple options.

By default, if you don't specify any initial state, the state machine
will be in the 'none' state and you would need to provide an event to
take it out of this state:

fsm = Fysom({
  'events': [
    {'name': 'startup', 'src': 'none',  'dst': 'green'},
    {'name': 'panic',   'src': 'green', 'dst': 'red'},
    {'name': 'calm',    'src': 'red',   'dst': 'green'},
  ]
})
print fsm.current # "none"
fsm.startup()
print fsm.current # "green"

If you specifiy the name of you initial event (as in all the earlier
examples), then an implicit 'startup' event will be created for you and
fired when the state machine is constructed:

fsm = Fysom({
  'initial': 'green',
  'events': [
    {'name': 'panic', 'src': 'green', 'dst': 'red'},
    {'name': 'calm',  'src': 'red',   'dst': 'green'},
  ]
})
print fsm.current # "green"

If your object already has a startup method, you can use a different
name for the initial event:

fsm = Fysom({
  'initial': {'state': 'green', 'event': 'init'},
  'events': [
    {'name': 'panic', 'src': 'green', 'dst': 'red'},
    {'name': 'calm',  'src': 'red',   'dst': 'green'},
  ]
})
print fsm.current # "green"

Finally, if you want to wait to call the initiall state transition
event until a later date, you can defer it:

fsm = Fysom({
  'initial': {'state': 'green', 'event': 'init', 'defer': True},
  'events': [
    {'name': 'panic', 'src': 'green', 'dst': 'red'},
    {'name': 'calm',  'src': 'red',   'dst': 'green'},
  ]
})
print fsm.current # "none"
fsm.init()
print fsm.current # "green"

Of course, we have now come full circle, this last example pretty much
functions the same as the first example in this section where you simply
define your own startup event.

So you have a number of choices available to you when initializing your
state machine.

"""

__author__ = 'Mansour Behabadi'
__copyright__ = 'Copyright 2011, Mansour Behabadi and Jake Gordon'
__credits__ = ['Mansour Behabadi', 'Jake Gordon']
__license__ = 'MIT'
__version__ = '1.0'
__maintainer__ = 'Mansour Behabadi'
__email__ = 'mansour@oxplot.com'

import types

try:
  unicode = unicode
except NameError:
  unicode = str
  basestring = (str, bytes)

class FysomError(Exception):
  pass

class Fysom(object):

  def __init__(self, cfg):
    self._apply(cfg)

  def isstate(self, state):
    return self.current == state

  def can(self, event):
    return event in self._map and self.current in self._map[event] \
      and not hasattr(self, 'transition')

  def cannot(self, event):
    return not self.can(event)

  def _apply(self, cfg):
    init = cfg['initial'] if 'initial' in cfg else None
    if isinstance(init, basestring):
      init = {'state': init}
    events = cfg['events'] if 'events' in cfg else []
    callbacks = cfg['callbacks'] if 'callbacks' in cfg else {}
    tmap = {}
    self._timers = cfg['timers'] if 'timers' in cfg else None
    for t in self._timers:
      t['state'] = 'stopped'
      t['timeleft'] = 0

    self._map = tmap

    def add(e):
      src = [e['src']] if isinstance(e['src'], basestring) else e['src']
      if e['name'] not in tmap:
        tmap[e['name']] = {}
      for s in src:
        tmap[e['name']][s] = e['dst']

    if init:
      if 'event' not in init:
        init['event'] = 'startup'
      add({'name': init['event'], 'src': 'none', 'dst': init['state']})

    for e in events:
      add(e)

    for name in tmap:
      setattr(self, name, self._build_event(name))

    for name in callbacks:
      setattr(self, name, callbacks[name])

    self.current = 'none'

    if init and 'defer' not in init:
      getattr(self, init['event'])()

  def _build_event(self, event):

    def fn(**kwargs):

      if hasattr(self, 'transition'):
        raise FysomError("event %s inappropriate because previous"
                         " transition did not complete" % event)
      if not self.can(event):
        raise FysomError("event %s inappropriate in current state"
                         " %s" % (event, self.current))

      src = self.current
      dst = self._map[event][src]

      class _e_obj(object):
        pass
      e = _e_obj()
      e.fsm, e.event, e.src, e.dst = self, event, src, dst
      for k in kwargs:
        setattr(e, k, kwargs[k])

      if hasattr(self, 'ontransition'):
        getattr(self,'ontransition')(e)

      if self.current != dst:

        if self._before_event(e) == False:
          return
        def _tran():
          delattr(self, 'transition')
          self.current = dst
          self._enter_state(e)
          self._change_state(e)
          self._after_event(e)
        self.transition = _tran

      if self._leave_state(e) != False:
        if hasattr(self, 'transition'):
          self.transition()

    return fn

  def _before_event(self, e):
    fnname = 'onbefore' + e.event
    if hasattr(self, fnname):
      return getattr(self, fnname)(e)

  def _after_event(self, e):
    for fnname in ['onafter' + e.event, 'on' + e.event]:
      if hasattr(self, fnname):
        return getattr(self, fnname)(e)

  def _leave_state(self, e):
    fnname = 'onleave' + e.src
    if hasattr(self, fnname):
      return getattr(self, fnname)(e)

  def _enter_state(self, e):
    for fnname in ['onenter' + e.dst, 'on' + e.dst]:
      if hasattr(self, fnname):
        return getattr(self, fnname)(e)

  def _change_state(self, e):
    fnname = 'onchangestate'
    if hasattr(self, fnname):
      return getattr(self, fnname)(e)

  def timer_expired(self,name,timer):
    print "timer ",name,"expired"
    e = "ontimer" + name + "_expired"
    if hasattr(self, e):
      result = getattr(self, e)(timer)
    e = name + "_expired"
    result = getattr(self, e)(origin=name)


    return False # True to re-arm

  def timer_start(self,name):
    for t in self._timers:
      if t['name'] ==  name:
        t['state'] = 'running'
        t['timeleft'] = t['duration']

  def timer_stop(self,name):
    for t in self._timers:
      if t['name'] == name:
        if t['state'] == 'running':
          t['state'] = 'stopped'
        else:
          print "timer_stop: timer",name,"not running"

  def timer_print(self):
    for t in self._timers:
      print t

  def jiffie(self, deltat):
    for t in self._timers:
      if t['state'] == 'running':
        t['timeleft'] -= deltat
        if t['timeleft'] <= 0:
           t['state'] = 'stopped'
           if self.timer_expired(t['name'],t):
             # reload timer
             t['state'] = 'running'
             t['timeleft'] = t['duration']

if __name__ == '__main__':
  pass
