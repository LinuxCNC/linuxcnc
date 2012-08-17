
import redis
print "connecting to redis server..",
r_server = redis.Redis("localhost")
print "connected."
r_server.set("who", "mhaberler was here")
print "key/value pair stored"
print "value retrieved:", r_server.get("who")
