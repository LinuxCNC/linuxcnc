#!/bin/bash
# The MDIs queued behind the yielding M400 remap run after it, in order
linuxcnc -r test.ini | awk '/^(pre|post) [0-9]|^self\.input_flag/ {sub(/ = .*/, ""); print}'
exit "${PIPESTATUS[0]}"
