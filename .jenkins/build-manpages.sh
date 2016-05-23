#!/bin/bash -e

cd /work/src
rm -rf ../man/doc/man9/*.asciidoc || /bin/true
sh autogen.sh
./configure --enable-build-documentation=asciidoc
make manpages i_manpages i_docpages


