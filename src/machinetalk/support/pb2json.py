#!/usr/bin/env python
# vim: sts=4 sw=4 et

"""
Patch Message class with two functions:
    SerializeToJSON which works like SerializeToString but results in JSON message
    ParseFromJSON which parses JSON message and fills object
Import this file to add functions
Also they may be used as json_encode and json_decode

Author: Pavel Shramov
see also: http://psha.org.ru/cgit/psha/pbufrpc

"""

from google.protobuf.message import Message


__all__ = ['json_encode', 'json_decode']

Message.SerializeToJSON = lambda s: json_encode(s)
Message.ParseFromJSON = lambda s, j: [s.Clear(), 1] and s.CopyFrom(json_decode(s, j))

def _load_simplejson():
    try:
        from simplejson import dumps, loads
        return dumps, loads
    except ImportError:
        return None, None

def _load_cjson():
    try:
        from cjson import encode, decode
        _encode = encode
        encode = lambda *a: _encode(*a).replace('\\/', '/') #XXX: Workaround #593302
        return encode, decode
    except ImportError:
        return None, None

_j_encode, _j_decode = None, None

def _load():
    global _j_encode, _j_decode
    if _j_encode:
        return
    _j_encode, _j_decode = _load_cjson()
    if _j_encode:
        return
    _j_encode, _j_decode = _load_simplejson()
    if not _j_encode:
        raise ImportError("No JSON implementation found")

def json_encode(obj):
    """ Encode Protobuf object (Message) in JSON """
    _load()
    return _j_encode(pb2jd(obj))

def json_decode(obj, str):
    """ Decode JSON message """
    _load()
    return jd2pb(obj.DESCRIPTOR, _j_decode(str or '{}'))

def pb2jd(o):
    def _pbe(d, obj):
        if d.type == d.TYPE_MESSAGE:
            return pb2jd(obj)
        elif d.type == d.TYPE_BYTES:
            return obj.encode('base64')
        else:
            return obj

    d = {}
    for f in o.DESCRIPTOR.fields:
        if f.label == f.LABEL_REPEATED:
            r = []
            for x in getattr(o, f.name):
                r.append(_pbe(f, x))
            d[f.name] = r
        elif f.label == f.LABEL_OPTIONAL:
            if o.HasField(f.name):
                d[f.name] = _pbe(f, getattr(o, f.name))
        elif f.label == f.LABEL_REQUIRED:
            if not o.HasField(f.name):
                raise ValueError("Required field %s is not found" % (f.name))
            d[f.name] = _pbe(f, getattr(o, f.name))
        else:
            raise TypeError("Unknown label type %s in %s" % (f.label, f.full_name))
    return d

def jd2pb(descriptor, jdict):
    def _pbd(d, obj):
        if d.type == d.TYPE_MESSAGE:
            return jd2pb(d.message_type, obj)
        elif d.type == d.TYPE_BYTES:
            return obj.decode('base64')
        else:
            return obj

    o = descriptor._concrete_class()
    for f in descriptor.fields:
        if f.label == f.LABEL_REPEATED:
            for x in jdict.get(f.name, []):
                if f.type == f.TYPE_MESSAGE:
                    getattr(o, f.name).add().CopyFrom(_pbd(f, x))
                else:
                    getattr(o, f.name).append(_pbd(f, x))
        elif f.label in [f.LABEL_OPTIONAL, f.LABEL_REQUIRED]:
            if f.name not in jdict:
                if f.label == f.LABEL_REQUIRED:
                    raise ValueError("Required field %s is not found" % (f.name))
                continue
            x = _pbd(f, jdict[f.name])
            if f.type == f.TYPE_MESSAGE:
                getattr(o, f.name).CopyFrom(x)
            else:
                setattr(o, f.name, x)
        else:
            raise TypeError("Unknown label type %s in %s" % (f.label, f.full_name))
    return o
