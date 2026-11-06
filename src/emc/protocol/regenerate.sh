#!/bin/sh
# Regenerate the committed C++ bindings from the schema.  flatc is needed
# only when emc.fbs changes; the build itself uses the generated header.
# The Rust crate in linuxcnc-nml regenerates its copy with `flatc --rust`.
set -e
cd "$(dirname "$0")"
flatc --cpp -o . emc.fbs
