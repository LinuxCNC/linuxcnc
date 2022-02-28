#!/bin/sh

${SUDO} halcompile --install test_shmem_rtcomp.comp

halrun -V setup.hal
