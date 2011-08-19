from interpreter import TOLERANCE_EQUAL

# Demo Python O-word subroutine - call as:
# o<square> [5]
# (debug, #<_value>)
#
# this function expects exactly one parameter and will throw an exception otherwise
def square(x):
    return  x*x

# a function taking a variable number of arguments
# o<multiply> [5] [7]
# (debug, #<_value>)
# o<multiply> [5] [7] [9] [16]
# (debug, #<_value>)

import operator

def multiply(*args):
    print "multiply: number of arguments=", len(args)
    return reduce(operator.mul, args)

