from fysom import Fysom



def printstatechange(e):
  print 'event: %s, src: %s, dst: %s' % (e.event, e.src, e.dst)

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

print fsm.current
print fsm.isstate('green'),  fsm.isstate('yellow')
print fsm.can('panic')

fsm.onchangestate = printstatechange

fsm.warn()
fsm.panic(msg='killer bees')
fsm.calm(msg='sedatives in the honey pots')
fsm.clear()
fsm.panic(msg='rodents')
fsm.calm(msg='cats ahead')
fsm.clear()
