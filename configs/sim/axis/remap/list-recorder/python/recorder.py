
profiles = dict()

def add(words):
    id = int(words["q"])
    del words["q"]

    print "profile",id,"words=",words

    if not profiles.has_key(id):
        profiles[id] = list()

    profiles[id].append(words)
    
