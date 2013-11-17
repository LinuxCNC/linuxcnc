#!/usr/bin/python2

from toolstore import toolstore

t = toolstore()

q = t.find_tool(3)
print q.keys()
print q
