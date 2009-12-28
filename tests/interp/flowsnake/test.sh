#!/bin/bash
rs274 -t test.tbl -g flowsnake.ngc | awk '{$1=""; print}'
