#!/bin/bash
./deprecated.py 2>&1 | sed -e's,^.*deprecated.py:[0-9]\+:\s*,,'
