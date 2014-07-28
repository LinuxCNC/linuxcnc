#!/bin/bash
#   This is a component of LinuxCNC
#   Copyright 2012 Michael Haberler <git@mah.priv.at>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#

# test for bug in libgl1-mesa-dri
# https://bugs.launchpad.net/ubuntu/+source/mesa/+bug/259219
# based Ulrich von Zadow's ldpreload_crash.zip posted there

# This will:
# exit 1 if the test failed to compile, or the workaround didnt work after all
# exit 0 if the test ran successfully
# if the LD_PRELOAD workaround is needed, echo the pathname of libstc++ on stdout


curdir=$PWD
tmpdir=`mktemp -d`
cd $tmpdir

cat  >ldpreload_main.c <<"EOFldpreload_main"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    void *handle = dlopen("./ldpreload_crash.so", RTLD_LOCAL | RTLD_NOW);
    if (!handle) {
        printf("dlopen failed with message '%s'\n", dlerror());
    }
    typedef void (*UseStreamPtr)();
    UseStreamPtr use_stream = (UseStreamPtr)(dlsym(handle, "use_stream"));
    if (use_stream) {
        use_stream();
    } else {
        printf("Function use_stream not found.\n");
	exit (1);
    }
    exit (0);
}

EOFldpreload_main

cat  >ldpreload_crash.cpp <<"EOFldpreload_crash"

#include <iostream>
#include <GL/gl.h>

using namespace std;

extern "C" {
    void use_stream()
    {
	cerr << "[foo]" << endl;
    }

    void use_gl()
    {
	// Make sure gl gets linked in.
	glEnable(GL_BLEND);
    }
}
EOFldpreload_crash

cat  >Makefile <<"EOFMakefile"
SHAREDLIBS = -lGL -lGLU -lstdc++

all: ldpreload_crash.so ldpreload_main

ldpreload_crash.so: ldpreload_crash.o
	c++ -fPIC -shared -o $@ $^ -rdynamic $(SHAREDLIBS)

ldpreload_main: ldpreload_main.o
	gcc -fPIC -o $@ $^ -ldl 

ldpreload_crash.o: %.o: %.cpp
	$(CXX) -fPIC -c $< -o $@

ldpreload_main.o: %.o: %.c
	gcc -c $< -o $@

EOFMakefile

make -s -i
if [ "$?" -ne "0" ]; then
  exit 1
fi

# 'extract' libstdc++ shared lib name - hope this is robust
libstdcpp=`ldd ldpreload_crash.so |grep stdc++|cut -d' ' -f3`

./ldpreload_main  >/dev/null 2>&1
if [ "$?" -ne "0" ]; then
    # the workaround is needed
    LD_PRELOAD=$libstdcpp ./ldpreload_main >/dev/null 2>&1 
    if [ "$?" -ne "0" ]; then
	exit 1
    fi
    # else echo pathname of the shared library which needs preloading
    echo $libstdcpp
    cd $curdir
    rm -rf $tmpdir
    exit 0
fi
# that worked, no workaround needed
cd $curdir
rm -rf $tmpdir
exit 0
