#!/bin/bash
set -x

# this one must succeed
rm -f names_match.c
comp names_match.comp
if [ $? -ne 0 ]; then
    echo 'comp failed to process names_match.comp'
    exit 1
fi
if [ ! -f names_match.c ]; then
    echo 'comp failed to produce names_match.c'
    exit 1
fi

# this one must fail
rm -f names_dont_match.c
comp names_dont_match.comp
if [ $? -eq 0 ]; then
    echo 'comp erroneously accepted names_dont_match.comp'
    exit 1
fi
if [ -f names_dont_match.c ]; then
    echo 'comp erroneously produced names_dont_match.c'
    exit 1
fi

