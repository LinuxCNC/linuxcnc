#!/usr/bin/python
#http://stackoverflow.com/questions/361/generate-list-of-all-possible-permutations-of-a-string
def nextPermutation(perm):
    k0 = None
    for i in range(len(perm)-1):
        if perm[i]<perm[i+1]:
            k0=i
    if k0 == None:
        return None

    l0 = k0+1
    for i in range(k0+1, len(perm)):
        if perm[k0] < perm[i]:
            l0 = i

    perm[k0], perm[l0] = perm[l0], perm[k0]
    perm[k0+1:] = reversed(perm[k0+1:])
    return perm


perm=["g88.1x1", "m405","m406","m407","m408"]

while perm:
    print perm
    perm = nextPermutation(perm)

perm=["g88.1x1", "m407","m408","m409","m410"]

while perm:
    print perm
    perm = nextPermutation(perm)
