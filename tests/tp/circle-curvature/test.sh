#!/bin/sh
set -e

# shellcheck disable=SC2086
${SUDO} halcompile --install curvaturecheck.c >/dev/null
halrun curvature.hal
echo 'circle curvature: OK'
