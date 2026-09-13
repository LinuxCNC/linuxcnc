#!/bin/sh
# Unit tests for the OpenGL 3.3 core / GLES 3.1 preview renderer shared by
# AXIS, the GTK screens (Gremlin/gmoccapy/gscreen/hal_gremlin) and QtVCP.
#
# None of them needs a GPU or an X display. The first two are GL-free
# outright - they load rs274.glcanon_bake by path and exercise it as plain
# numpy. The third imports rs274.glcanon, which pulls PyOpenGL at import time
# (it never calls into it here), and PyOpenGL is not installed in a headless
# build - so that one is conditional, the same rule tests/pyvcp/skip applies
# to a whole directory.
set -e

python3 test_backplot_palette.py >&2
python3 test_camera_matrices.py >&2

if python3 -c 'import OpenGL' 2>/dev/null; then
    python3 test_workpiece.py >&2
else
    echo "skip: test_workpiece.py needs PyOpenGL (headless build)" >&2
fi

echo ok
