cmake_minimum_required(VERSION 3.0)
set(PROJECT "inivar")
project(${PROJECT} C)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/libnml)
include_directories(${SRC_DIR}/../../src/libnml/inifile)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCES
    ${SRC_DIR}/../../src/libnml/inifile/inivar.cc
    ${SRC_DIR}/../../src/libnml/inifile/inifile.cc
    )
add_executable(${PROJECT_NAME} ${SOURCES})

add_definitions(-DSIM -DULAPI -UULAPI -DRTAPI)

# Set include paths for libraries to link.
target_include_directories(${PROJECT_NAME} PRIVATE ${BUILD_DIR})

# Set include paths for libraries to link.
add_library(linuxcncini SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcncini PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcncini/liblinuxcncini.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
    )

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE
    pthread -pthread -lrt -ludev -ldl linuxcncini
    )

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../bin)

# Notes.

