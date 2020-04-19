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
    if g++ -std=c++11 -S -o /dev/null -xcxx /dev/null > /dev/null 2>&1; then
        ELEVEN=-std=c++11
    else
        ELEVEN=-std=c++0x
    fi
    g++ $ELEVEN -DULAPI -I$HEADERS -E -x c++ $i > /dev/null
done
