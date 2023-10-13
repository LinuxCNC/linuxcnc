cmake_minimum_required(VERSION 3.0)
# Component name.
set(PROJECT "scurve") # The name of the file excluding .c or .h
project(${PROJECT} C CXX ASM) # Asm is optional.

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR}/../../../src/rtapi)
include_directories(${SRC_DIR}/../../../src/hal)
include_directories(${SRC_DIR}/../../../src/emc/motion)
include_directories(${SRC_DIR}/../../../src/emc/nml_intf)
include_directories(${SRC_DIR}/../../../src/emc/kinematics)
include_directories(${SRC_DIR}/../../../src/emc/tp)
include_directories(${SRC_DIR}/../../../src/libnml/posemath)

# Compile flags.
SET(GCC_COVERAGE_COMPILE_FLAGS "-I. -I${RTAPI_DIR} -I${HAL_DIR} \
    -DUSPACE -fno-fast-math -fno-unsafe-math-optimizations -DRTAPI -D_GNU_SOURCE
    -Drealtime -D__MODULE__ -DSIM -fPIC -Os")
add_definitions(-DRTAPI -DUSPACE -DRTAPI -D_GNU_SOURCE -D__MODULE__ )

# No "lib" before filename.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

ADD_LIBRARY(${PROJECT} SHARED
    ${SRC_DIR}/sc_engine.h
    ${SRC_DIR}/sc_engine.c
    ${SRC_DIR}/scurve.h
    ${SRC_DIR}/scurve.c
)

# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
  IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcnchal/liblinuxcnchal.so"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../../include/"
)

# Set include paths for libraries to link.
#add_library(linuxcncini SHARED IMPORTED) # or STATIC instead of SHARED
#set_target_properties(linuxcncini PROPERTIES
##  IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcncini/liblinuxcncini.so"
#  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../liblinuxcncini/"
#)

# Compile link with libhalcore.so or linuxcnchal.so
target_include_directories(${PROJECT_NAME} PRIVATE "${BUIlD_DIR}/src/hal/${RTLINK}")
target_link_libraries(${PROJECT_NAME} PRIVATE pthread rt linuxcnchal ${RTLINK})

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../../rtlib)
