cmake_minimum_required(VERSION 3.0)
set(PROJECT "linuxcnc_tlc")
project(${PROJECT} LANGUAGES CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
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
include_directories(${SRC_DIR}/../../src/emc/usr_intf)
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
include_directories(/usr/include/tcl8.6)
include_directories(/usr/lib/x86_64-linux-gnu)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

ADD_LIBRARY(${PROJECT} SHARED
    ${SRC_DIR}/../../src/emc/usr_intf/emcsh.cc
    ${SRC_DIR}/../../src/emc/usr_intf/shcom.cc
    )

add_definitions(-DULAPI)

# Set include paths for libraries to link.
add_library(linuxcnc_ STATIC IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnc_ PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnc/liblinuxcnc.a"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

add_library(nml SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(nml PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/libnml/libnml.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )


add_library(linuxcncini_ SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcncini_ PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcncini/liblinuxcncini.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE
    linuxcncini_
    linuxcnc_
    nml
    -ltclstub8.6  -lX11 -lXss -lXext -lXft -lfontconfig -lfreetype -lfontconfig  -lpthread -ldl -lz  -lpthread -lm  -lXinerama
    -ltcl8.6 -ltk8.6
    )

# Set custom output name :
set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME linuxcnc)

# Make install
# install(TARGETS ${PROJECT} DESTINATION ${SRC_DIR})

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../tcl)
