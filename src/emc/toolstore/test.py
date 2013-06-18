#!/usr/bin/python2

from toolstore import toolstore

t = toolstore()
q = t.choose_offset_by_num(int("0203"))
print q.keys()
print q

