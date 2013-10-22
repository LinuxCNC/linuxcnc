#!/bin/sh
set -xe 
HEADERS=$(readlink -f ../../../include)
for i in $HEADERS/*.h; do
    case $i in
    */rtapi_app.h) continue ;;
    esac
    gcc -DULAPI -I$HEADERS -E -x c $i > /dev/null
done
for i in $HEADERS/*.h $HEADERS/*.hh; do
    case $i in
    */rtapi_app.h) continue ;;
    */interp_internal.hh) continue ;;
    esac
    g++ -DULAPI -I$HEADERS -E -x c++ $i > /dev/null
done
