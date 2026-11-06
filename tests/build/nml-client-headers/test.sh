#!/bin/bash
# The NML client headers are installed flat, together with the libraries they
# need.  Compile and link a client against the installed set only; that is the
# build an out-of-tree UI of this kind has to be able to do.
set -e

g++ -std=c++17 -Wall -Wextra -Werror -I"${HEADERS}" -c client.cc -o client.o
g++ -o client client.o -L"${LIBDIR}" -Wl,-rpath,"${LIBDIR}" \
    -llinuxcnc -lnml -llinuxcncini

rm -f client client.o
exit 0
