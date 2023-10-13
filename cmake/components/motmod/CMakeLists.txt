cmake_minimum_required(VERSION 3.0)
set(PROJECT "motmod")
set(CMAKE_CXX_STANDARD 20) # Needed by ruckig.
project(${PROJECT} CXX C)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR}/../../../src)
include_directories(${SRC_DIR}/../../../src/rtapi)
include_directories(${SRC_DIR}/../../../src/hal)
include_directories(${SRC_DIR}/../../../src/emc/motion)
include_directories(${SRC_DIR}/../../../src/emc/nml_intf)
include_directories(${SRC_DIR}/../../../src/emc/kinematics)
include_directories(${SRC_DIR}/../../../src/emc/tp)
include_directories(${SRC_DIR}/../../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../vendor/ruckig)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Set compile flags.
SET(GCC_COVERAGE_COMPILE_FLAGS "-I. -I${RTAPI_DIR} -I${HAL_DIR} \
    -DUSPACE -fno-fast-math -fno-unsafe-math-optimizations -DRTAPI -D_GNU_SOURCE
    -Drealtime -D__MODULE__ -DSIM -fPIC -Os")
add_definitions(-DRTAPI -DUSPACE -D_GNU_SOURCE -D__MODULE__ )
    
ADD_LIBRARY(${PROJECT} SHARED 
    ${SRC_DIR}/../../../src/emc/kinematics/cubic.c
    ${SRC_DIR}/../../../src/emc/motion/axis.c
    ${SRC_DIR}/../../../src/emc/motion/motion.c
    ${SRC_DIR}/../../../src/emc/motion/command.c
    ${SRC_DIR}/../../../src/emc/motion/control.c
    ${SRC_DIR}/../../../src/emc/motion/simple_tp.h
    ${SRC_DIR}/../../../src/emc/motion/simple_tp.c
    ${SRC_DIR}/../../../src/emc/motion/emcmotutil.c
    ${SRC_DIR}/../../../src/emc/motion/stashf.c
    ${SRC_DIR}/../../../src/emc/motion/dbuf.c
    ${SRC_DIR}/../../vendor/ruckig/cpp_interface.h
    ${SRC_DIR}/../../vendor/ruckig/cpp_interface.cpp
    ${SRC_DIR}/../../vendor/ruckig/brake.hpp
    ${SRC_DIR}/../../vendor/ruckig/brake.cpp
    ${SRC_DIR}/../../vendor/ruckig/position-step1.cpp
    ${SRC_DIR}/../../vendor/ruckig/position-step2.cpp
    ${SRC_DIR}/../../vendor/ruckig/velocity-step1.cpp
    ${SRC_DIR}/../../vendor/ruckig/velocity-step2.cpp

    #${SRC_DIR}/../sc_tp_src/ruckig/block.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/brake.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/brake.cpp
    #${SRC_DIR}/../sc_tp_src/ruckig/calculator.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/calculator_online.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/calculator_target.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/input_parameter.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/output_parameter.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/position.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/position-step1.cpp
    #${SRC_DIR}/../sc_tp_src/ruckig/position-step2.cpp
    #${SRC_DIR}/../sc_tp_src/ruckig/profile.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/reflexxes_comparison.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/roots.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/ruckig.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/trajectory.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/utils.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/velocity.hpp
    #${SRC_DIR}/../sc_tp_src/ruckig/velocity-step1.cpp
    #${SRC_DIR}/../sc_tp_src/ruckig/velocity-step2.cpp
)

# Set path to eigen3, for install $ sudo apt-get install libeigen3-dev
include_directories("/usr/include/eigen3/Eigen")

# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
  IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcnchal/liblinuxcnchal.so"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../../include"
)

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE linuxcnchal)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../../rtlib)

# Notes.

