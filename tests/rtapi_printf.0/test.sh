#!/bin/bash -xe

g++ -DULAPI -I"${EMC2_HOME}/src/rtapi" -I"${HEADERS}" -std=c++0x \
    -DSIM -rdynamic -L"${LIBDIR}" \
    -o test_rtapi_vsnprintf test_rtapi_vsnprintf.c
./test_rtapi_vsnprintf
