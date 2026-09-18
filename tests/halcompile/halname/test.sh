#!/bin/bash

# Two declarations that claim one HAL name must be rejected here, not left to
# fail at loadrt as "HAL: ERROR: duplicate pin". Pins and params share one
# namespace in hal_lib.c; functions have their own, but hal_export_funct()
# also creates <funct>.time, .tmax and .tmax-increased as a pin and params.
# An array claims one name per element, and is limited to 256 elements.
for c in collide_pin_pin collide_pin_param collide_function collide_array \
         collide_funct_time collide_personality array_limit; do
    rm -f "$c.c"
    halcompile --preprocess "$c.comp" 2>&1 && echo "halcompile accepted $c.comp"
    [ -f "$c.c" ] && echo "halcompile produced $c.c"
done

# A pin and a function that mangle to one name are exported into different
# HAL namespaces, so they do not collide.
c=separate_namespaces
rm -f "$c.c"
halcompile --preprocess "$c.comp" 2>&1 || echo "halcompile rejected $c.comp"
[ -f "$c.c" ] || echo "halcompile did not produce $c.c"
