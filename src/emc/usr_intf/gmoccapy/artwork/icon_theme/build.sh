#!/usr/bin/env sh
pushd "$(dirname "$(realpath "$0")")"
# build each icon theme like this:
# python3 -m ${THEME_DIR}.build $@
python3 -m material.build $@
popd