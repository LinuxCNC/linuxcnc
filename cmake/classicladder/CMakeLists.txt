cmake_minimum_required(VERSION 3.0)
set(PROJECT "classicladder")
project(${PROJECT} CXX C) 

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/rtapi)
include_directories(${SRC_DIR}/../../src/hal)
include_directories(${SRC_DIR}/../../src/hal/classicladder)

# Gtk include dirs.
include_directories(/usr/local/include/freetype2)

# Set prefix.
# set(CMAKE_SHARED_LIBRARY_PREFIX "")

include_directories(/usr/local/include/freetype2)
include_directories(/usr/local/include)
include_directories(/usr/include/gtk-2.0)
include_directories(/usr/lib/x86_64-linux-gnu/gtk-2.0/include)
include_directories(/usr/include/pango-1.0)
include_directories(/usr/include/atk-1.0)
include_directories(/usr/include/gdk-pixbuf-2.0)
include_directories(/usr/include/pango-1.0)
include_directories(/usr/include/libmount)
include_directories(/usr/include/blkid)
include_directories(/usr/include/fribidi)
include_directories(/usr/include/cairo)
include_directories(/usr/include/pixman-1)
include_directories(/usr/include/harfbuzz)
include_directories(/usr/include/glib-2.0)
include_directories(/usr/lib/x86_64-linux-gnu/glib-2.0/include)
include_directories(/usr/include/uuid)
include_directories(/usr/include/libpng16)

set(SOURCES
    ${SRC_DIR}/../../src/hal/classicladder/arithm_eval.c
    ${SRC_DIR}/../../src/hal/classicladder/arrays.c
    ${SRC_DIR}/../../src/hal/classicladder/calc.c
    ${SRC_DIR}/../../src/hal/classicladder/calc_sequential.c
    ${SRC_DIR}/../../src/hal/classicladder/classicladder.c
    ${SRC_DIR}/../../src/hal/classicladder/classicladder_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/config_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/drawing.c
    ${SRC_DIR}/../../src/hal/classicladder/drawing_sequential.c
    ${SRC_DIR}/../../src/hal/classicladder/edit.c
    ${SRC_DIR}/../../src/hal/classicladder/edit_copy.c
    ${SRC_DIR}/../../src/hal/classicladder/edit_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/edit_sequential.c
    ${SRC_DIR}/../../src/hal/classicladder/editproperties_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/emc_mods.c
    ${SRC_DIR}/../../src/hal/classicladder/files.c
    ${SRC_DIR}/../../src/hal/classicladder/files_project.c
    ${SRC_DIR}/../../src/hal/classicladder/files_sequential.c
    ${SRC_DIR}/../../src/hal/classicladder/manager.c
    ${SRC_DIR}/../../src/hal/classicladder/manager_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/menu_and_toolbar_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/print_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/protocol_modbus_master.c
    ${SRC_DIR}/../../src/hal/classicladder/protocol_modbus_slave.c
    ${SRC_DIR}/../../src/hal/classicladder/serial_linux.c
    ${SRC_DIR}/../../src/hal/classicladder/socket_modbus_master.c
    ${SRC_DIR}/../../src/hal/classicladder/socket_server.c
    ${SRC_DIR}/../../src/hal/classicladder/spy_vars_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/symbols.c
    ${SRC_DIR}/../../src/hal/classicladder/symbols_gtk.c
    ${SRC_DIR}/../../src/hal/classicladder/vars_names.c
    ${SRC_DIR}/../../src/hal/classicladder/vars_access.c
    )

add_executable(${PROJECT_NAME} ${SOURCES})

add_definitions(-DULAPI -DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER
    -DSEQUENTIAL_SUPPORT -DHAL_SUPPORT -DDYNAMIC_PLCSIZE -DRT_SUPPORT -DOLD_TIMERS_MONOS_SUPPORT -DMODBUS_IO_MASTER
    # -DGNOME_PRINT_USE
    -DGTK_INTERFACE -DGTK2 -Wno-deprecated-declarations)

# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
  IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnchal/liblinuxcnchal.so"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../include"
)

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE linuxcnchal -lpthread
    -L/usr/local/lib -lgtk-x11-2.0 -lgdk-x11-2.0 -lpangocairo-1.0 -latk-1.0 -lcairo -lgdk_pixbuf-2.0
    -lgio-2.0 -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0 -lharfbuzz -lfontconfig -lfreetype
    )

# GTK2_VERSION = 2.24.33
# GTK2_CFLAGS = -pthread -I/usr/local/include/freetype2 -I/usr/local/include -I/usr/include/gtk-2.0 -I/usr/lib/x86_64-linux-gnu/gtk-2.0/include -I/usr/include/pango-1.0 -I/usr/include/atk-1.0 -I/usr/include/gdk-pixbuf-2.0 -I/usr/include/pango-1.0 -I/usr/include/libmount -I/usr/include/blkid -I/usr/include/fribidi -I/usr/include/cairo -I/usr/include/pixman-1 -I/usr/include/harfbuzz -I/usr/include/glib-2.0 -I/usr/lib/x86_64-linux-gnu/glib-2.0/include -I/usr/include/uuid -I/usr/include/libpng16
# GTK2_LIBS = -L/usr/local/lib -lgtk-x11-2.0 -lgdk-x11-2.0 -lpangocairo-1.0 -latk-1.0 -lcairo -lgdk_pixbuf-2.0 -lgio-2.0 -lpangoft2-1.0 -lpango-1.0 -lgobject-2.0 -lglib-2.0 -lharfbuzz -lfontconfig -lfreetype
# HAVE_GNOMEPRINT =

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../bin)

# Notes.
