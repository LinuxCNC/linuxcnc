#!/bin/bash
rs274 -t test.tbl -g inverse.ngc | awk '{$1=""; print}'
