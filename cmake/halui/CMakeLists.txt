cmake_minimum_required(VERSION 3.0)
set(PROJECT "halui") # ~/linuxcnc/src/emc/nml_inft
project(${PROJECT} VERSION 0 LANGUAGES CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/rtapi)
include_directories(${SRC_DIR}/../../src/hal)
include_directories(${SRC_DIR}/../../src/libnml/rcs)
include_directories(${SRC_DIR}/../../src/libnml/nml)
include_directories(${SRC_DIR}/../../src/libnml/cms)
include_directories(${SRC_DIR}/../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../src/libnml/inifile)
include_directories(${SRC_DIR}/../../src/libnml/os_intf)
include_directories(${SRC_DIR}/../../src/emc/nml_intf)
include_directories(${SRC_DIR}/../../src/emc/tooldata)
include_directories(${SRC_DIR}/../../src/emc/usr_intf)
include_directories(${SRC_DIR}/../../src/emc/motion)
include_directories(${SRC_DIR}/../../src/emc/rs274ngc)
include_directories(${SRC_DIR}/../../src/emc)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCES
    ${SRC_DIR}/../../src/emc/usr_intf/halui.cc
    )
add_executable(${PROJECT_NAME} ${SOURCES})
add_definitions(-DULAPI)

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

add_library(linuxcnc_ STATIC IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnc_ PROPERTIES
  # IMPORTED_LOCATION "/home/user/lcncladdertest/lib/liblinuxcnc.a"
  IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnc/liblinuxcnc.a"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
)

target_link_libraries(${PROJECT_NAME} nml linuxcnchal linuxcncini linuxcnc_ tooldata)

# Add verion nr. .so.0.0.0
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../bin)

# Notes.








