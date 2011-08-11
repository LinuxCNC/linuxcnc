from interpreter import TOLERANCE_EQUAL

# need to deal with float inaccuracies
def eq(a,b):
    return abs(a -b) < TOLERANCE_EQUAL

# return tuple (bool<is_near_int>,<nearest_int>)
def is_near_int(value):
    result = int(value + 0.5)
    return (abs(result - value) < TOLERANCE_EQUAL,result)


# Demo Python O-word subroutine - call as:
# o<square> [5]
# (debug, #<_value>)
#
# len(args) reflects the number of actual parameters passed
def square(args):
    print "argc=", len(args)
    return args[0]*args[0]

# a function taking a variable number of arguments
# (in principle... modulo the assert statement)
# o<multiply> [5] [7]
# (debug, #<_value>)
# o<multiply> [5] [7] [9] [16]
# (debug, #<_value>)

import operator

def multiply(*args):
    assert len(*args) == 3
    return reduce(operator.mul, *args)
