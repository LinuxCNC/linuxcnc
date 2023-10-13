cmake_minimum_required(VERSION 3.0)
set(PROJECT "linuxcnc") # ~/linuxcnc/src/emc/nml_inft
project(${PROJECT} VERSION 0 LANGUAGES CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/rtapi)
include_directories(${SRC_DIR}/../../src/emc)
include_directories(${SRC_DIR}/../../src/hal)
include_directories(${SRC_DIR}/../../src/emc/nml_intf)
include_directories(${SRC_DIR}/../../src/emc/ini)
include_directories(${SRC_DIR}/../../src/emc/rs274ngc)
include_directories(${SRC_DIR}/../../src/emc/motion)
include_directories(${SRC_DIR}/../../src/emc/tooldata)
include_directories(${SRC_DIR}/../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../src/libnml/rcs)
include_directories(${SRC_DIR}/../../src/libnml/nml)
include_directories(${SRC_DIR}/../../src/libnml/cms)
include_directories(${SRC_DIR}/../../src/libnml/linklist)
include_directories(${SRC_DIR}/../../src/libnml/inifile)

# Set prefix.
# set(CMAKE_SHARED_LIBRARY_PREFIX "")

ADD_LIBRARY(${PROJECT} STATIC # Mention static.
    ${SRC_DIR}/../../src/emc/nml_intf/emc.hh
    ${SRC_DIR}/../../src/emc/nml_intf/emcglb.c
    ${SRC_DIR}/../../src/emc/rs274ngc/modal_state.cc
    ${SRC_DIR}/../../src/emc/nml_intf/emc.cc
    ${SRC_DIR}/../../src/emc/nml_intf/emcpose.c
    ${SRC_DIR}/../../src/emc/nml_intf/emcargs.cc
    ${SRC_DIR}/../../src/emc/nml_intf/emcops.cc
    ${SRC_DIR}/../../src/emc/nml_intf/canon_position.cc
    ${SRC_DIR}/../../src/emc/nml_intf/interpl.cc
    ${SRC_DIR}/../../src/emc/ini/emcIniFile.cc
    ${SRC_DIR}/../../src/emc/ini/iniaxis.cc
    ${SRC_DIR}/../../src/emc/ini/inijoint.cc
    ${SRC_DIR}/../../src/emc/ini/inispindle.cc
    ${SRC_DIR}/../../src/emc/ini/initraj.cc
    ${SRC_DIR}/../../src/emc/ini/inihal.cc
    )

add_definitions(-DULAPI)

# Set include paths for libraries to link.
target_include_directories(${PROJECT_NAME} PRIVATE ${BUILD_DIR})

# Add verion nr. .so.0.0.0
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

# Solves compile error for /tcl/linuxcnc.so lib.
set(CMAKE_CXX_FLAGS "-fPIC ${CMAKE_CXX_FLAGS}")

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../lib)

# Notes.



