#!/bin/bash

sed -i subs/test1.ngc \
    -e '/HERE/ a ;'
