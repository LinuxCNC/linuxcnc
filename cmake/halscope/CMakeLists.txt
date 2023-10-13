cmake_minimum_required(VERSION 3.0)
set(PROJECT "halscope")
project(${PROJECT} CXX C) 

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/rtapi)
include_directories(${SRC_DIR}/../../src/hal)

# Gtk include dirs.
include_directories(/usr/local/include/freetype2)
include_directories(/usr/local/include)
include_directories(/usr/include/gtk-3.0)
include_directories(/usr/include/at-spi2-atk/2.0)
include_directories(/usr/include/at-spi-2.0)
include_directories(/usr/include/dbus-1.0)
include_directories(/usr/lib/x86_64-linux-gnu/dbus-1.0/include)
include_directories(/usr/include/gtk-3.0)
include_directories(/usr/include/gio-unix-2.0)
include_directories(/usr/include/cairo)
include_directories(/usr/include/pango-1.0)
include_directories(/usr/include/fribidi)
include_directories(/usr/include/harfbuzz)
include_directories(/usr/include/atk-1.0)
include_directories(/usr/include/pixman-1)
include_directories(/usr/include/uuid)
include_directories(/usr/include/libpng16)
include_directories(/usr/include/gdk-pixbuf-2.0)
include_directories(/usr/include/libmount)
include_directories(/usr/include/blkid)
include_directories(/usr/include/glib-2.0)
include_directories(/usr/lib/x86_64-linux-gnu/glib-2.0/include)
include_directories(/usr/include/x86_64-linux-gnu)
# Set prefix.
# set(CMAKE_SHARED_LIBRARY_PREFIX "")

set(SOURCES
    ${SRC_DIR}/../../src/hal/utils/scope.c
    ${SRC_DIR}/../../src/hal/utils/scope_horiz.c
    ${SRC_DIR}/../../src/hal/utils/scope_vert.c
    ${SRC_DIR}/../../src/hal/utils/scope_trig.c
    ${SRC_DIR}/../../src/hal/utils/scope_disp.c
    ${SRC_DIR}/../../src/hal/utils/scope_files.c
    ${SRC_DIR}/../../src/hal/utils/miscgtk.c
)

add_executable(${PROJECT_NAME} ${SOURCES})

add_definitions(-DULAPI)

# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
  IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnchal/liblinuxcnchal.so"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
)

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE linuxcnchal -pthread
    -lgtk-3 -lgdk-3 -lpangocairo-1.0 -lpango-1.0 -lharfbuzz -latk-1.0
    -lcairo-gobject -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lgobject-2.0
    -lglib-2.0 -lm
)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../bin)

# Notes.
