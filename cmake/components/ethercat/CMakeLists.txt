# Mention
# This file cointains 2 cmake projects.
# 1. lcec       (ethercat driver)
# 2. lcec_conf  (executable, cq helper program to load a xml config file into the driver)
# 3. Copy the xml file to the rtlib or in your machine config dir.

# Todo:
# This cmake ethercat driver is not tested on real hardware yet.

cmake_minimum_required(VERSION 3.0)
set(PROJECT "lcec") 
project(${PROJECT} CXX C) 

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../../src/rtapi)
include_directories(${SRC_DIR}/../../../src/hal)
include_directories(${SRC_DIR}/../../vendor/ethercat)


# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Set compile flags.
SET(GCC_COVERAGE_COMPILE_FLAGS "-I. -I${RTAPI_DIR} -I${HAL_DIR} \
    -DUSPACE -fno-fast-math -fno-unsafe-math-optimizations -DRTAPI -D_GNU_SOURCE
    -Drealtime -D__MODULE__ -DSIM -fPIC -Os")
add_definitions(-DRTAPI -DUSPACE -D_GNU_SOURCE -D__MODULE__ )

ADD_LIBRARY(${PROJECT} SHARED
    ${SRC_DIR}/../../vendor/ethercat/lcec_main.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_class_enc.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_generic.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_ax5200.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el1xxx.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el1252.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el1859.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el2xxx.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el2202.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el31x2.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el31x4.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el3255.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el40x1.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el40x2.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el40x8.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el41x2.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el41x4.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el5101.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el5151.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el5152.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el2521.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el6900.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el1918_logic.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el1904.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el2904.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el7041_1000.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el70x1.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el7211.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el7342.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el7411.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_el95xx.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_em7004.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_stmds5k.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_deasda.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_omrg5.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_ph3lm2rm.c
    )
# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcnchal/liblinuxcnchal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../../include/"
    )

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE PRIVATE pthread rt linuxcnchal)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../../rtlib)

# Notes.

cmake_minimum_required(VERSION 3.0)
set(PROJECT "lcec_conf")
project(${PROJECT} CXX C)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../../src/rtapi)
include_directories(${SRC_DIR}/../../../src/hal)
include_directories(${SRC_DIR}/../../vendor/ethercat)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

# Set compile flags.
SET(GCC_COVERAGE_COMPILE_FLAGS "-I. -I${RTAPI_DIR} -I${HAL_DIR} \
    -DUSPACE -fno-fast-math -fno-unsafe-math-optimizations -DRTAPI -D_GNU_SOURCE
    -Drealtime -D__MODULE__ -DSIM -fPIC -Os")
add_definitions(-DRTAPI -DUSPACE -D_GNU_SOURCE -D__MODULE__ )

set(SOURCES
    ${SRC_DIR}/../../vendor/ethercat/lcec_conf.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_conf_util.c
    ${SRC_DIR}/../../vendor/ethercat/lcec_conf_icmds.c
    )
add_executable(${PROJECT_NAME} ${SOURCES})

# Set include paths for libraries to link.
add_library(linuxcnchal_ SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal_ PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcnchal/liblinuxcnchal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../../include/"
    )

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE PRIVATE pthread rt linuxcnchal_ expat)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../../rtlib)

# Notes.

