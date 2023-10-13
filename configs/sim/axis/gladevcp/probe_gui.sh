#!/bin/bash

gladevcp -d -d -u probe.py -U debug=3 -H probe.hal probe.ui &
simulate_probe
