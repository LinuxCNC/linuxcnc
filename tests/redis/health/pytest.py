#!/usr/bin/python

import redis

rc = redis.Redis(host='localhost', port=6579)
t = rc.get('mypasswd');

print t,
