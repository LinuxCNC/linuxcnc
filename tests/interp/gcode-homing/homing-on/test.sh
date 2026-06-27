#!/bin/bash
rs274 -i test.ini -g test.ngc | awk '{$1=""; print}' | sed 's/-0\.0000/0.0000/g'
exit "${PIPESTATUS[0]}"
