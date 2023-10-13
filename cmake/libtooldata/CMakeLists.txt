cmake_minimum_required(VERSION 3.0)
set(PROJECT "libtooldata") 
project(${PROJECT} VERSION 0 LANGUAGES CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/emc)
include_directories(${SRC_DIR}/../../src/emc/tooldata)
include_directories(${SRC_DIR}/../../src/hal)
include_directories(${SRC_DIR}/../../src/hal/utils)
include_directories(${SRC_DIR}/../../src/rtapi)
include_directories(${SRC_DIR}/../../src/emc/nml_intf)
include_directories(${SRC_DIR}/../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../src/emc/motion)
include_directories(${SRC_DIR}/../../src/emc/rs274ngc)
include_directories(${SRC_DIR}/../../src/libnml/nml)
include_directories(${SRC_DIR}/../../src/libnml/rcs)
include_directories(${SRC_DIR}/../../src/libnml/cms)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

ADD_LIBRARY(${PROJECT} SHARED 
    ${SRC_DIR}/../../src/emc/tooldata/tooldata_mmap.cc
    ${SRC_DIR}/../../src/emc/tooldata/tooldata_common.cc
    ${SRC_DIR}/../../src/emc/tooldata/tooldata_db.cc
)

add_definitions(-DULAPI)

# Add verion nr. .so.0.0.0
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

# Set include paths for libraries to link.
target_include_directories(${PROJECT_NAME} PRIVATE ${BUILD_DIR})

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../lib)
