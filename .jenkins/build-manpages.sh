#!/bin/bash -e

cd /work/src
rm -rf ../man/man9/*.asciidoc || /bin/true
sh autogen.sh
./configure --enable-build-documentation
make docpages i_docpages


