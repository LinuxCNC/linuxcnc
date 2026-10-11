#!/bin/bash
rs274 -g g68.ngc | sed 's/-0\.0000/0.0000/g'
exit "${PIPESTATUS[0]}"
