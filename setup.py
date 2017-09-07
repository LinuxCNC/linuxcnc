#!/usr/bin/env python

from distutils.core import setup

# See README for usage instructions.
import os
import subprocess
import sys
import shutil

PROJECT = 'machinetalk'
PROJECT_NAME = '%s-protobuf' % PROJECT
DESCRIPTION = "Protobuf Python modules for %s" % PROJECT
VERSION = "v1.0.7"
AUTHOR = "Alexander Roessler"
AUTHOR_EMAIL = "alex@machinekoder.com"
PROJECT_URL = 'https://github.com/machinekit/%s' % PROJECT_NAME
DOWNLOAD_URL = 'https://github.com/machinekit/%s/archive/%s.tar.gz' % (PROJECT_NAME, VERSION)

# We must use setuptools, not distutils, because we need to use the
# namespace_packages option for the "google" package.
try:
  from setuptools import setup, Extension, find_packages
except ImportError:
  try:
    from ez_setup import use_setuptools
    use_setuptools()
    from setuptools import setup, Extension, find_packages
  except ImportError:
    sys.stderr.write(
        "Could not import setuptools; make sure you have setuptools or "
        "ez_setup installed.\n"
    )
    raise

from distutils.command.clean import clean as _clean

if sys.version_info[0] == 3:
  # Python 3
  from distutils.command.build_py import build_py_2to3 as _build_py
else:
  # Python 2
  from distutils.command.build_py import build_py as _build_py
from distutils.spawn import find_executable

# Find the Protocol Compiler.
if 'PROTOC' in os.environ and os.path.exists(os.environ['PROTOC']):
  protoc = os.environ['PROTOC']
elif os.path.exists("./src/protoc"):
  protoc = "./src/protoc"
elif os.path.exists("./src/protoc.exe"):
  protoc = "./src/protoc.exe"
elif os.path.exists("./vsprojects/Debug/protoc.exe"):
  protoc = "./vsprojects/Debug/protoc.exe"
elif os.path.exists("./vsprojects/Release/protoc.exe"):
  protoc = "./vsprojects/Release/protoc.exe"
else:
  protoc = find_executable("protoc")

google_protobuf_includedir = subprocess.check_output(["pkg-config", "--variable=includedir", "protobuf"]).decode().strip()

def generate_proto(source, require = True):
  """Invokes the Protocol Compiler to generate a _pb2.py from the given
  .proto file.  Does nothing if the output already exists and is newer than
  the input."""

  if not require and not os.path.exists(source):
    return

  output = source.replace(".proto", "_pb2.py").replace("./src/", "")

  if (not os.path.exists(output) or
      (os.path.exists(source) and
       os.path.getmtime(source) > os.path.getmtime(output))):
    print("Generating %s..." % output)

    if not os.path.exists(source):
      sys.stderr.write("Can't find required file: %s\n" % source)
      sys.exit(-1)

    if protoc is None:
      sys.stderr.write(
          "protoc is not installed nor found in ./src. "
          "Please compile it or install the binary package.\n"
      )
      sys.exit(-1)

    protoc_command = [protoc, "-I./src", "-I" + google_protobuf_includedir, "--python_out=.", source]
    print("Command: %s" % protoc_command)
    if subprocess.call(protoc_command) != 0:
      sys.exit(-1)

def create_init(path):
  if not os.path.exists(path):
    os.mkdir(path)

  initfile = os.path.join(path, '__init__.py')
  if not os.path.exists(initfile):
    content = "__import__('pkg_resources').declare_namespace(__name__)\n"
    with open(initfile, 'w') as f:
      f.write(content)

class clean(_clean):
  def run(self):
    # delete _init_ files
    shutil.rmtree(PROJECT)

    # Delete generated files in the code tree.
    for (dirpath, dirnames, filenames) in os.walk("."):
      for filename in filenames:
        filepath = os.path.join(dirpath, filename)
        if filepath.endswith("_pb2.py") or filepath.endswith(".pyc"):
          os.remove(filepath)
    # _clean is an old-style class, so super() doesn't work.
    _clean.run(self)


class build_py(_build_py):
  def run(self):
    source_path = './src/%s/protobuf/' % PROJECT

    # Generate necessary .proto file if it doesn't exist.
    for entry in os.listdir(source_path):
      filepath = os.path.join(source_path, entry)
      if os.path.isfile(filepath) and filepath.endswith('.proto'):
          generate_proto(filepath)

    # _build_py is an old-style class, so super() doesn't work.
    _build_py.run(self)

if __name__ == '__main__':
      # create __init__ files
      create_init(PROJECT)
      create_init(PROJECT + '/protobuf')
      # start the setup
      setup(name=PROJECT_NAME,
            version=VERSION,
            description=DESCRIPTION,
            url=PROJECT_URL,
            download_url=DOWNLOAD_URL,
            author=AUTHOR,
            author_email=AUTHOR_EMAIL,
            namespace_packages=[PROJECT],
            packages=find_packages(),
            install_requires=['setuptools', 'protobuf'],
            cmdclass={
                'clean': clean,
                'build_py': build_py,
            }
            )
