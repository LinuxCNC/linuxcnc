#!/usr/bin/env python3
# vim: sts=4 sw=4 et

"""
HAL type and direction constants for Python.

In GOMC, userspace HAL components are managed by gomc-server (Go modules
or cmod plugins). This module provides only the type/direction constants
needed by legacy Python code (e.g. pyvcp_widgets).
"""

# Pin/param types
HAL_BIT = 1
HAL_FLOAT = 2
HAL_S32 = 3
HAL_U32 = 4

# Pin directions
HAL_IN = 16
HAL_OUT = 32
HAL_IO = 48