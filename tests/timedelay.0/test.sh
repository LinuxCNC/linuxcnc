#!/bin/bash
# timedelay captured via the filestream cmod (was WS sampler). not.0 + timedelay
# form a free-running oscillator; capture 64 tagged samples of in/out.
. "$(dirname "$0")/../filestream-driver.sh"
fs_run timedelay.hal
