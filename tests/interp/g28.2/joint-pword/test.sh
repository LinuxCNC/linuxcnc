#!/bin/bash
# G28.2 Pn homes a single joint, G28.2 P-1 homes them all (no INI flag needed).
rs274 -g test.ngc | awk '{$1=""; print}' | sed 's/-0\.0000/0.0000/g'
exit "${PIPESTATUS[0]}"
