#!/bin/sh
${SUDO} halcompile --install rtapi_app_main_fails.comp
halrun -v setup.hal
RETVAL=$?

if [ $RETVAL -eq 0 ]; then
    echo "the module loaded, but shouldn't have"
    exit 1
fi

echo "the module failed to load, just like it should"
exit 0
