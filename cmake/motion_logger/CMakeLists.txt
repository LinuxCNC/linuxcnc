cmake_minimum_required(VERSION 3.0)
set(PROJECT "motion-logger")
set(CMAKE_CXX_STANDARD 20) # Needed by ruckig.
project(${PROJECT} LANGUAGES CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR}/../../src)
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
include_directories(${SRC_DIR}/../../src/libnml/rcs)
include_directories(${SRC_DIR}/../../src/libnml/nml)
include_directories(${SRC_DIR}/../../src/libnml/cms)
include_directories(${SRC_DIR}/../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../src/libnml/inifile)
include_directories(${SRC_DIR}/../../src/libnml/os_intf)
include_directories(${SRC_DIR}/../vendor/ruckig)

# Set prefix.
# set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCES
    ${SRC_DIR}/../../src/emc/motion-logger/motion-logger.c
    ${SRC_DIR}/../../src/emc/motion/axis.c
    ${SRC_DIR}/../../src/emc/motion/simple_tp.c
    ${SRC_DIR}/../vendor/ruckig/cpp_interface.h
    ${SRC_DIR}/../vendor/ruckig/cpp_interface.cpp
    ${SRC_DIR}/../vendor/ruckig/cpp_interface.h
    ${SRC_DIR}/../vendor/ruckig/cpp_interface.cpp
    ${SRC_DIR}/../vendor/ruckig/brake.hpp
    ${SRC_DIR}/../vendor/ruckig/brake.cpp
    ${SRC_DIR}/../vendor/ruckig/position-step1.cpp
    ${SRC_DIR}/../vendor/ruckig/position-step2.cpp
    ${SRC_DIR}/../vendor/ruckig/velocity-step1.cpp
    ${SRC_DIR}/../vendor/ruckig/velocity-step2.cpp
    )
add_executable(${PROJECT_NAME} ${SOURCES})

add_definitions(-DULAPI)

# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnchal/liblinuxcnchal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(nml SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(nml PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/libnml/libnml.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE nml linuxcnchal -lm) # -lm, m=math for sqrt.

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../bin)

# Notes.
