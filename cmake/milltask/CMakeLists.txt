cmake_minimum_required(VERSION 3.0)
set(PROJECT "milltask")
set(CMAKE_CXX_STANDARD 20)
project(${PROJECT} LANGUAGES C CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/emc/sai)
include_directories(${SRC_DIR}/../../src/emc/task)
include_directories(${SRC_DIR}/../../src/emc/pythonplugin)
include_directories(${SRC_DIR}/../../src/hal)
include_directories(${SRC_DIR}/../../src/rtapi)
include_directories(${SRC_DIR}/../../src/emc)
include_directories(${SRC_DIR}/../../src/emc/kinematics)
include_directories(${SRC_DIR}/../../src/emc/motion-logger)
include_directories(${SRC_DIR}/../../src/emc/tooldata)
include_directories(${SRC_DIR}/../../src/emc/iotask)
include_directories(${SRC_DIR}/../../src/emc/nml_intf)
include_directories(${SRC_DIR}/../../src/emc/motion)
include_directories(${SRC_DIR}/../../src/emc/rs274ngc)
include_directories(${SRC_DIR}/../../src/emc/tp)
include_directories(${SRC_DIR}/../../src/emc/ini)
include_directories(${SRC_DIR}/../../src/libnml/rcs)
include_directories(${SRC_DIR}/../../src/libnml/nml)
include_directories(${SRC_DIR}/../../src/libnml/cms)
include_directories(${SRC_DIR}/../../src/libnml/buffer)
include_directories(${SRC_DIR}/../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../src/libnml/inifile)
include_directories(${SRC_DIR}/../../src/libnml/os_intf)
include_directories(/usr/include/python3.9)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCES
    ${SRC_DIR}/../../src/emc/motion/emcmotglb.c
    ${SRC_DIR}/../../src/emc/task/emctask.cc
    ${SRC_DIR}/../../src/emc/task/emccanon.cc
    ${SRC_DIR}/../../src/emc/task/emctaskmain.cc
    ${SRC_DIR}/../../src/emc/motion/usrmotintf.cc
    ${SRC_DIR}/../../src/emc/motion/emcmotutil.c
    ${SRC_DIR}/../../src/emc/task/taskintf.cc
    ${SRC_DIR}/../../src/emc/motion/dbuf.c
    ${SRC_DIR}/../../src/emc/motion/stashf.c
    ${SRC_DIR}/../../src/emc/task/taskclass.cc
    ${SRC_DIR}/../../src/emc/task/backtrace.cc
    )
add_executable(${PROJECT_NAME} ${SOURCES})

add_definitions(-DULAPI)

# Set include paths for libraries to link.
add_library(nml SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(nml PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/libnml/libnml.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnchal/liblinuxcnchal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(linuxcncini SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcncini PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcncini/liblinuxcncini.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(tooldata SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(tooldata PROPERTIES
    # IMPORTED_LOCATION "/home/user/lcncladdertest/lib/libtooldata.so"
    IMPORTED_LOCATION "${SRC_DIR}/../build/libtooldata/libtooldata.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(linuxcnc_a STATIC IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnc_a PROPERTIES
    # IMPORTED_LOCATION "/home/user/lcncladdertest/lib/liblinuxcnc.a"
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnc/liblinuxcnc.a"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(rs274_ SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(rs274_ PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/librs274/librs274.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(pyplugin SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(pyplugin PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/libpyplugin/libpyplugin.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include" # Todo: figur out the excact includes for this one.
    )

add_library(posemath SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(posemath PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/libposemath/libposemath.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE
    nml linuxcnchal linuxcncini tooldata linuxcnc_a rs274_ pyplugin posemath
    -I/usr/lib -lpython3.9
    -lboost_python39 -lcrypt -lpthread -ldl -lutil -lm -lm -lreadline
    )

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../bin)

# Notes.
