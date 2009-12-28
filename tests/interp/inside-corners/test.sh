#!/bin/bash
rs274 -g test.ngc | awk '{$1=""; print}'
