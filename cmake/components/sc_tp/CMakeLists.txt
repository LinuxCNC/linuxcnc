cmake_minimum_required(VERSION 3.0)
set(PROJECT "sc_tp")
set(CMAKE_CXX_STANDARD 20) # Needed by ruckig.
project(${PROJECT} CXX C) 

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(RTLINK "liblinuxcnchal")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../../src)
include_directories(${SRC_DIR}/../../../src/rtapi)
include_directories(${SRC_DIR}/../../../src/hal)
include_directories(${SRC_DIR}/../../../src/emc/tp)
include_directories(${SRC_DIR}/../../../src/emc/motion)
include_directories(${SRC_DIR}/../../../src/emc/nml_intf)
include_directories(${SRC_DIR}/../../../src/emc/kinematics)
include_directories(${SRC_DIR}/../../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../vendor/ruckig)

# Set path to eigen3, for install $ sudo apt-get install libeigen3-dev
include_directories("/usr/include/eigen3/Eigen")

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Set compile flags.
SET(GCC_COVERAGE_COMPILE_FLAGS "-I. -I${RTAPI_DIR} -I${HAL_DIR} \
    -DUSPACE -fno-fast-math -fno-unsafe-math-optimizations -DRTAPI -D_GNU_SOURCE
    -Drealtime -D__MODULE__ -DSIM -fPIC -Os")
add_definitions(-DRTAPI -DUSPACE -D_GNU_SOURCE -D__MODULE__ )

ADD_LIBRARY(${PROJECT} SHARED 
    ${SRC_DIR}/../../../src/emc/tp/tp.h
    ${SRC_DIR}/../../../src/emc/motion/motion.h
    ${SRC_DIR}/../sc_tp_src/sc_tp.c
    ${SRC_DIR}/../sc_tp_src/sc_struct.h
    ${SRC_DIR}/../sc_tp_src/sc_extern.h
    ${SRC_DIR}/../sc_tp_src/sc_struct.cpp
    ${SRC_DIR}/../sc_tp_src/sc_arcs.h
    ${SRC_DIR}/../sc_tp_src/sc_arcs.cpp
    ${SRC_DIR}/../sc_tp_src/sc_lines.h
    ${SRC_DIR}/../sc_tp_src/sc_lines.cpp
    ${SRC_DIR}/../sc_tp_src/sc_block.h
    ${SRC_DIR}/../sc_tp_src/sc_block.c
    ${SRC_DIR}/../sc_tp_src/sc_optimizer.h
    ${SRC_DIR}/../sc_tp_src/sc_optimizer.cpp
    ${SRC_DIR}/../sc_tp_src/sc_conversion.h
    ${SRC_DIR}/../sc_tp_src/sc_conversion.c
    ${SRC_DIR}/../sc_tp_src/sc_vector.h
    ${SRC_DIR}/../sc_tp_src/sc_vector.cpp
    ${SRC_DIR}/../sc_tp_src/sc_ruckig.h
    ${SRC_DIR}/../sc_tp_src/sc_ruckig.cpp
    ${SRC_DIR}/../../vendor/ruckig/block.hpp
    ${SRC_DIR}/../../vendor/ruckig/brake.hpp
    ${SRC_DIR}/../../vendor/ruckig/brake.cpp
    ${SRC_DIR}/../../vendor/ruckig/calculator.hpp
    ${SRC_DIR}/../../vendor/ruckig/calculator_online.hpp
    ${SRC_DIR}/../../vendor/ruckig/calculator_target.hpp
    ${SRC_DIR}/../../vendor/ruckig/input_parameter.hpp
    ${SRC_DIR}/../../vendor/ruckig/output_parameter.hpp
    ${SRC_DIR}/../../vendor/ruckig/position.hpp
    ${SRC_DIR}/../../vendor/ruckig/position-step1.cpp
    ${SRC_DIR}/../../vendor/ruckig/position-step2.cpp
    ${SRC_DIR}/../../vendor/ruckig/profile.hpp
    ${SRC_DIR}/../../vendor/ruckig/reflexxes_comparison.hpp
    ${SRC_DIR}/../../vendor/ruckig/roots.hpp
    ${SRC_DIR}/../../vendor/ruckig/ruckig.hpp
    ${SRC_DIR}/../../vendor/ruckig/trajectory.hpp
    ${SRC_DIR}/../../vendor/ruckig/utils.hpp
    ${SRC_DIR}/../../vendor/ruckig/velocity.hpp
    ${SRC_DIR}/../../vendor/ruckig/velocity-step1.cpp
    ${SRC_DIR}/../../vendor/ruckig/velocity-step2.cpp

    )

# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcnchal/liblinuxcnchal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../../include/"
    )

add_library(posemath SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(posemath PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../../build/libposemath/libposemath.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../../include/"
    )


# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE linuxcnchal posemath)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../../rtlib)

# Notes.
