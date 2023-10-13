cmake_minimum_required(VERSION 3.5)
set(PROJECT "qt_occt") # The name of the file excluding .c or .h
project(${PROJECT} C CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths for header files etc.
include_directories(${SRC_DIR}/../)
include_directories(${SRC_DIR}/../../)
include_directories(${SRC_DIR}/../../vendor)
include_directories(${SRC_DIR}/../../../src)
include_directories(${SRC_DIR}/../qt_occt_src/)
# target_include_directories(${PROJECT_NAME} PRIVATE "/usr/local/lib")

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

# Kdl
include_directories("/usr/include/eigen3")
find_package (Eigen3 3.3 REQUIRED)

include_directories("/usr/local/include/kdl")
#find_package (orocos-kdl 1.0 REQUIRED)

include_directories("/usr/include/eigen3")
find_package (Eigen3 3.3 REQUIRED)

# Set include paths for libraries to link.
add_library(kdl SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(kdl PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../../vendor/orocos_kinematics_dynamics/orocos_kdl/build/src/liborocos-kdl.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../vendor/orocos_kinematics_dynamics/orocos_kdl/src"
    )

# Set include paths for libraries to link.
add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcnchal/liblinuxcnchal.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../../include"
    )
#[[
add_library(linuxcncini SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcncini PROPERTIES
  IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcncini/liblinuxcncini.so"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../liblinuxcncini_src"
)

add_library(linuxcnc_a STATIC IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnc_a PROPERTIES
  # IMPORTED_LOCATION "/home/user/lcncladdertest/lib/liblinuxcnc.a"
  IMPORTED_LOCATION "${SRC_DIR}/../../build/liblinuxcnc/liblinuxcnc.a"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../liblinuxcnc_src"
)

# Set include paths for libraries to link.
add_library(nml SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(nml PROPERTIES
  IMPORTED_LOCATION "${SRC_DIR}/../../build/libnml/libnml.so"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../libnml_src"
)

add_library(rs274_ SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(rs274_ PROPERTIES
    IMPORTED_LOCATION "${SRC_DIR}/../../build/librs274/librs274.so"
    INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../../librs274_src"
    )]]

# Opencascade
find_package (OpenCASCADE REQUIRED)

INCLUDE_DIRECTORIES(${OpenCASCADE_INCLUDE_DIR})
SET(OpenCASCADE_LIBS
    TKGeomAlgo TKMesh TKHLR TKBO TKShHealing
    TKPrim
    TKernel TKMath TKTopAlgo TKService
    TKG2d TKG3d TKV3d TKOpenGl
    TKBRep TKXSBase TKGeomBase TKGeomAlgo
    TKXSDRAW
    TKLCAF TKXCAF TKCAF TKVCAF
    TKCDF TKBin TKBinL TKBinXCAF TKXml TKXmlL TKXmlXCAF
    # -- IGES support
    TKIGES
    # -- STEP support
    TKSTEP TKXDESTEP TKXDEIGES TKSTEPAttr TKSTEPBase TKSTEP209
    # -- STL support
    TKSTL
    # -- OBJ/glTF support
    TKRWMesh TKMeshVS
    # -- VRML support
    TKVRML
    # -- ViewerTest
    TKViewerTest
    )

# Qt
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_PREFIX_PATH "/opt/qt-creator/5.15.1/gcc_64/lib/cmake")
include_directories("/opt/qt-creator/5.15.1/gcc_64/include/QtWidgets")
include_directories("/opt/qt-creator/5.15.1/gcc_64/include/QtGui")
include_directories("/opt/qt-creator/5.15.1/gcc_64/include")
include_directories("/opt/qt-creator/5.15.1/gcc_64/include/QtCore")

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets OpenGL REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets OpenGL REQUIRED)

include_directories("${SRC_DIR}")
include_directories("usr/local/include/opencascade")
include_directories("usr/include")
include_directories("usr/local/lib")

set(PROJECT_SOURCES
    ${SRC_DIR}/../qt_occt_src/main.cpp
    ${SRC_DIR}/../qt_occt_src/mainwindow.cpp
    ${SRC_DIR}/../qt_occt_src/mainwindow.h
    ${SRC_DIR}/../qt_occt_src/mainwindow.ui
    ${SRC_DIR}/../qt_occt_src/hal/halio.h
    ${SRC_DIR}/../qt_occt_src/hal/halio.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_base.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_classes.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_entities.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_header.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_interface.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_objects.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/dx_data.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/dx_iface.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_cptable932.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_cptable936.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_cptable949.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_cptable950.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_cptables.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_dbg.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_textcodec.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgbuffer.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader15.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader18.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader21.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader24.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader27.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgutil.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dxfreader.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dxfwriter.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/rscodec.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/libdwgr.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/libdxfrw.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/libdxfrw_functions.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/main_doc.h
    ${SRC_DIR}/../qt_occt_src/libgcode/gcode_program.h
    ${SRC_DIR}/../qt_occt_src/libgcode/interface.h
    ${SRC_DIR}/../qt_occt_src/libgcode/parser.h
    ${SRC_DIR}/../qt_occt_src/libocct/draw_primitives.h
    ${SRC_DIR}/../qt_occt_src/libocct/opencascade.h
    ${SRC_DIR}/../qt_occt_src/libskynet/char_to_string.h
    ${SRC_DIR}/../qt_occt_src/libskynet/directory.h
    ${SRC_DIR}/../qt_occt_src/libspline/bezier_spline.h
    ${SRC_DIR}/../qt_occt_src/libspline/cubic_spline.h
    ${SRC_DIR}/../qt_occt_src/libspline/spline.h
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_classes.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_entities.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_header.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/drw_objects.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/dx_iface.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_dbg.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/drw_textcodec.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgbuffer.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader15.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader18.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader21.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader24.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgreader27.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dwgutil.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dxfreader.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/dxfwriter.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/intern/rscodec.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/libdwgr.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/libdxfrw.cpp
    ${SRC_DIR}/../qt_occt_src/libdxfrw/libdxfrw_functions.cpp
    ${SRC_DIR}/../qt_occt_src/libgcode/gcode_program.cpp
    ${SRC_DIR}/../qt_occt_src/libgcode/interface.cpp
    ${SRC_DIR}/../qt_occt_src/libgcode/parser.cpp
    ${SRC_DIR}/../qt_occt_src/libocct/draw_primitives.cpp
    ${SRC_DIR}/../qt_occt_src/libocct/opencascade.cpp
    ${SRC_DIR}/../qt_occt_src/libskynet/char_to_string.cpp
    ${SRC_DIR}/../qt_occt_src/libskynet/directory.cpp
    ${SRC_DIR}/../qt_occt_src/libspline/bezier_spline.cpp
    ${SRC_DIR}/../qt_occt_src/libspline/cubic_spline.cpp
    ${SRC_DIR}/../qt_occt_src/libspline/spline.cpp
    ${SRC_DIR}/../qt_occt_src/libkinematic/kinematic.h
    ${SRC_DIR}/../qt_occt_src/libkinematic/kinematic.h
    )

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(${PROJECT}
        ${PROJECT_SOURCES}
        )
else()
    if(ANDROID)
        add_library(${PROJECT} SHARED
            ${PROJECT_SOURCES}
            )
    else()
        add_executable(${PROJECT}
            ${PROJECT_SOURCES}
            )
    endif()
endif()

# Link.
target_link_libraries(${PROJECT_NAME} PRIVATE pthread rt
    # linuxcnchal linuxcncini linuxcnc_a nml rs274_
    kdl linuxcnchal
    ${OpenCASCADE_LIBS} Qt5::Widgets Qt5::OpenGL ${OPENGL_LIBRARIES} ${GLUT_LIBRARY}
    )

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../../bin)
