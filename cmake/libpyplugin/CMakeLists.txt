cmake_minimum_required(VERSION 3.0)
set(PROJECT "libpyplugin")
project(${PROJECT} VERSION 0 LANGUAGES CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/emc/pythonplugin)

#[[
PYTHON_CPPFLAGS=-I/usr/include/python3.9
PYTHON_EXTRA_LIBS=-lcrypt -lpthread -ldl  -lutil -lm -lm
PYTHON_EXTRA_LDFLAGS=-Xlinker -export-dynamic -Wl,-O1 -Wl,-Bsymbolic-functions
PYTHON_LIBS=-lcrypt -lpthread -ldl  -lutil -lm -lm
BOOST_PYTHON_LIB=-lboost_python39
PYTHON_CPPFLAGS=-I/usr/include/python3.9
PYTHON_LIBS=-L/usr/lib -lpython3.9
]]
include_directories(/usr/include/python3.9)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

ADD_LIBRARY(${PROJECT} SHARED
    ${SRC_DIR}/../../src/emc/pythonplugin/python_plugin.cc
    )

add_definitions(-DULAPI)

# Set include paths for libraries to link.
add_library(linuxcncini SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcncini PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcncini/liblinuxcncini.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../src/libnml/inifile"
)

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE linuxcncini -fPIC
    -I/usr/lib -lpython3.9
    -lstdc++
    -lboost_python39 -lcrypt -lpthread -ldl -lutil -lm -lm
    -Xlinker -export-dynamic -Wl,-O1 -Wl,-Bsymbolic-functions
)

# Add verion nr. .so.0.0.0
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../lib)

# Notes.
