#!/bin/sh
if test -x ../../bin/test_rtapi_vsnprintf
then
    test_rtapi_vsnprintf
else
    exit 0
fi
