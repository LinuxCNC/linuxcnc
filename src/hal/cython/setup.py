#!/usr/bin/env python
# vim: sts=4 sw=4 et

from setuptools import setup
#from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

setup( name = "ce"
     , cmdclass = {'build_ext': build_ext}
     , packages = ["linuxcnc"]
     , include_dirs = ["../../hal", "../../rtapi"]
     , ext_modules =
            [Extension("linuxcnc.hal", ["linuxcnc/hal.pyx"], libraries=["linuxcnchal"])
            ]
     , test_suite = 'nose.collector'
)
