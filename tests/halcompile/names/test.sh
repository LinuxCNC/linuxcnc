#!/bin/bash
set -x

# this one must succeed
rm -f names_match.c
modcompile --preprocess names_match.comp -o names_match.c
if [ $? -ne 0 ]; then
    echo 'modcompile failed to process names_match.comp'
    exit 1
fi
if [ ! -f names_match.c ]; then
    echo 'modcompile failed to produce names_match.c'
    exit 1
fi

# this one must fail
rm -f names_dont_match.c
modcompile --preprocess names_dont_match.comp -o names_dont_match.c
if [ $? -eq 0 ]; then
    echo 'modcompile erroneously accepted names_dont_match.comp'
    exit 1
fi
if [ -f names_dont_match.c ]; then
    echo 'modcompile erroneously produced names_dont_match.c'
    exit 1
fi

