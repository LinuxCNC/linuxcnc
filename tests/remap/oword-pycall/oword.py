# Demo Python O-word subroutine - call as:
# o<square> [5]
# (debug, #<_value>)
#
# this function expects exactly one parameter from NGC and will throw an exception otherwise
# which aint particularly bright
def square(self,x):
    return  x*x

# a function taking a variable number of arguments
# o<multiply> [5] [7]
# (debug, #<_value>)
# o<multiply> [5] [7] [9] [16]
# (debug, #<_value>)

import operator

# you'd be better of doing it this way:
def multiply(self, *args):
    print "multiply: number of arguments=", len(args)
    return reduce(operator.mul, args)

