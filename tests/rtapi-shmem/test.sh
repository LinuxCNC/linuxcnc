#!/bin/bash
# rtapi shmem validation. singleton removed (gomc: all comps multi-instance) — the
# comp now loads as a single named instance. Builds via modcompile; the shmem
# init/zero/free/realloc check runs in EXTRA_SETUP at load.
${SUDO} modcompile --install test_shmem_rtcomp.comp || exit 1
gomc-server -f setup.hal
