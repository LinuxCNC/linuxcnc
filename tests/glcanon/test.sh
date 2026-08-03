#!/bin/sh
# Unit tests for the OpenGL 3.3 core / GLES 3.1 preview renderer shared by
# AXIS, the GTK screens (Gremlin/gmoccapy/gscreen/hal_gremlin) and QtVCP.
#
# Both are GL-free: they exercise the backplot palette and the explicit camera
# matrices as plain numpy, so they need neither a GPU nor an X display.
set -e

python3 test_backplot_palette.py >&2
python3 test_camera_matrices.py >&2

echo ok
