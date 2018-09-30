from distutils.core import setup
from distutils.extension import Extension
from Cython.Build import cythonize
import sysconfig


 
extentions = [
    Extension("py3hal", ["py3hal.pyx"],
              include_dirs=['../../../include'],
              libraries=["../../../lib/linuxcnchal"],
              extra_compile_args = ['-DULAPI']
    )
]
setup(
    name="component",
    ext_modules = cythonize(extentions,annotate=True),
)
