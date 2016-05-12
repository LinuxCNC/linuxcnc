#!/bin/bash -e

cd /work/src
sh autogen.sh
./configure --enable-build-documentation=asciidoc
make manpages i_manpages i_docpages


