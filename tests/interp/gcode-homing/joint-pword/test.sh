#!/bin/bash
# G28.2 Pn homes a single joint (no INI flag needed, same as bare G28.2).
rs274 -g test.ngc | awk '{$1=""; print}' | sed 's/-0\.0000/0.0000/g'
exit "${PIPESTATUS[0]}"
