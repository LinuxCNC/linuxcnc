#!/bin/bash

# Two declarations that claim one HAL name must be rejected here, not left to
# fail at loadrt as "HAL: ERROR: duplicate pin". Pins and params share one
# namespace in hal_lib.c; functions have their own, but hal_export_funct()
# creates <funct>.time, .tmax and .tmax-increased as a pin and params.
# An array claims one name per element. The same 'if' condition on both sides
# is still a collision: whatever creates one creates the other.
for c in collide_pin_pin collide_pin_param collide_function \
         collide_array collide_funct_time collide_same_condition; do
    rm -f $c.c
    if halcompile --preprocess $c.comp 2>&1; then
        echo "halcompile erroneously accepted $c.comp"
    fi
    if [ -f $c.c ]; then
        echo "halcompile erroneously produced $c.c"
    fi
done

# A pin and a function that mangle to one name are NOT a collision: they are
# exported into different HAL namespaces. Two declarations under different
# personality conditions are not one either -- they need not ever coexist, and
# the expressions cannot be evaluated at compile time.
for c in separate_namespaces condition_exclusive; do
    rm -f $c.c
    if ! halcompile --preprocess $c.comp 2>&1; then
        echo "halcompile rejected $c.comp"
    fi
    if [ ! -f $c.c ]; then
        echo "halcompile failed to produce $c.c"
    fi
done
