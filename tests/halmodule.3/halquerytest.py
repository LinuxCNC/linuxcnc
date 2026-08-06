#!/usr/bin/env python3

import hal

# There is a chance that component IDs are not always consistent. They
# should be, but there are no guarantees. Therefore, we simply mask the
# number and use 'ID' as replacement.
def fix_comp_id(d):
    if 'comp_id' in d:
        d['comp_id'] = "ID"
    return d

def fix_id(d):
    if 'id' in d:
        d['id'] = "ID"
    return d

# Call the listing query methods.
print("# Pins")
for n,v in hal.query.pins().items():
    print(n, fix_comp_id(v))

print("# Params")
for n,v in hal.query.params().items():
    print(n, fix_comp_id(v))

print("# Signals")
for n,v in hal.query.signals().items():
    print(n, v)

print("# Components")
for n,v in hal.query.comps().items():
    if not n.startswith("halcmd"):  # halcmd add a component 'halcmd<pid>'
        print(n, fix_id(v))

print("# Functions")
for n,v in hal.query.functs().items():
    print(n, fix_comp_id(v))

print("# Threads")
for n,v in hal.query.threads().items():
    print(n, fix_comp_id(v))

print("# Signal pins")
for n,v in hal.query.signalpins('net-xor-a').items():
    print(n, fix_comp_id(v))

print("# Intended failures")
if None is hal.query.pin('does-not-exist'):
    print("pin('does-not-exist') fails correctly")
if None is hal.query.param('does-not-exist'):
    print("param('does-not-exist') fails correctly")
if None is hal.query.signal('does-not-exist'):
    print("signal('does-not-exist') fails correctly")
if None is hal.query.comp('does-not-exist'):
    print("comp('does-not-exist') fails correctly")
if None is hal.query.comp(65534):
    print("comp(65534) fails correctly")
if None is hal.query.funct('does-not-exist'):
    print("funct('does-not-exist') fails correctly")
if None is hal.query.thread('does-not-exist'):
    print("thread('does-not-exist') fails correctly")
if None is hal.query.signalpins('does-not-exist'):
    print("signalpins('does-not-exist') fails correctly")

# Call the named query methods
print("# Simple named queries")
print("Pin:", fix_comp_id(hal.query.pin('or2.0.in1')))
print("Param:", fix_comp_id(hal.query.param('testthread.tmax')))
print("Signal:", hal.query.signal('net-output'))
print("Component:", fix_id(hal.query.comp('and2')))
print("Function:", fix_comp_id(hal.query.funct('xor2.0')))
print("Thread:", fix_comp_id(hal.query.thread('testthread')))
