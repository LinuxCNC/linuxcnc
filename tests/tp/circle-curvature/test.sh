#!/bin/sh
set -e

halcompile --install curvaturecheck.c >/dev/null
halrun curvature.hal
echo 'circle curvature: OK'
