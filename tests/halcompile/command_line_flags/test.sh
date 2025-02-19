#!/bin/sh
if [ -e flags ]
then
    rm flags
fi
halcompile --userspace --compile flags.c > /dev/null
if [ -e flags ]
then
    echo "No flags, compile succeeded but should fail"
    exit 1
fi
halcompile --userspace --compile --extra-compile-args="-I/usr/include/readline" flags.c > /dev/null
if [ -e flags ]
then
    echo "Linker flags only, compile succeeded but should fail"
    exit 1
fi
halcompile --userspace --compile --extra-link-args="-lm -lreadline" flags.c > /dev/null
if [ -e flags ]
then
    echo "Compiler flags only, compile succeeded but should fail"
    exit 1
fi
halcompile --userspace --compile --extra-compile-args="-I/usr/include/readline" --extra-link-args="-lm -lreadline" flags.c > /dev/null
if [ -e flags ]
then
    rm flags
    exit 0
else
    echo "Compile failed but should succeed"
    exit 1
fi

