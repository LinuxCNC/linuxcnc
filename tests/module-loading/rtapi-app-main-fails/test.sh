#!/bin/sh
# A component whose init returns an error must fail to load.
#
# Classic used `option rtapi_app no` + a custom rtapi_app_main() returning
# -ERANGE.  gomc has no rtapi_app / rt-userspace split -- a cmod's New() is its
# init -- so the component fails its init via a failing EXTRA_SETUP instead.
# When New() returns non-zero, `load` must fail (launcher: "factory returned
# error code").
set -e
${SUDO} modcompile --install rtapi_app_main_fails.comp
if halrun -f setup.hal; then
    echo "the module loaded, but shouldn't have"
    exit 1
fi
echo "the module failed to load, just like it should"
exit 0
