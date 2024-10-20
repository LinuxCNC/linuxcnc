#!/bin/bash -e

if ! command -v cppcheck; then
  echo "E: Please install the program 'cppcheck' prior to executing this script."
  exit 1
fi

nproc=2
if command -v nproc; then
  nproc=$(nproc)
fi

# *** C dominated folder ***
echo -n "I (1/4): checking HAL folders with C code only from dir "; pwd
for d in src/hal/classicladder src/hal/components; do
  (cd $d && cppcheck -j $nproc --language=c --force *.h *.c)
done

echo -n "I (2/4): checking EMC folders with C code only from dir "; pwd
for d in src/emc/motion-logger src/emc/tp
do
    (cd $d && cppcheck -j $nproc --language=c --force *.h *.c)
done
# problematic: src/hal/drivers src/hal/user_comps src/hal/utits

# *** C++ dominated folder ***
echo -n "I (3/4): checking EMC folders with C++ code only from dir "; pwd
for d in src/emc/canterp src/emc/ini src/emc/pythonplugin src/emc/tooldata
do
    (cd $d && cppcheck -j $nproc --language=c++ --force *.hh *.cc)
done
# problematic iotask src/emc/sai src/emc/task

# *** C++ and C++ in same folder ***
echo -n "I (4/4): checking EMC folders with both C and C++ code from dir "; pwd
for d in src/emc/rs274ngc;
do
    (cd $d && cppcheck -j $nproc --language=c --force *.h *.c && cppcheck -j $nproc --language=c++ --force *.hh *.cc)
done
# problematic src/emc/kinematics src/emc/nml_intf src/emc/usr_intf
