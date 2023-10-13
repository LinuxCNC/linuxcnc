cmake_minimum_required(VERSION 3.5)
set(PROJECT "myApp") # The name of the file excluding .c or .h
set(RT_CORE "halcore") # Link to halcore or for Lcnc use linuxcnchal
project(${PROJECT} C CXX ASM) # Asm is optional.

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(RTAPI_DIR "${SRC_DIR}/../../../../rtapi")
set(HAL_DIR "${SRC_DIR}/../../../../hal")
set(LIB_DIR "${SRC_DIR}/../../../../../lib")
set(RT_DIR "${SRC_DIR}/../../../../../rtlib")
# If user decides to create a build dir use the path.
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths for header files etc.
include_directories(${SRC_DIR})
include_directories(${RTAPI_DIR})
include_directories(${HAL_DIR})
include_directories(${LIB_DIR})

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

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
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/mainwindow.h
    ${SRC_DIR}/mainwindow.ui
    ${SRC_DIR}/cplusplus.h
    ${SRC_DIR}/cplusplus.cpp
    ${SRC_DIR}/hal/halio.h
    ${SRC_DIR}/hal/halio.cpp
    ${SRC_DIR}/libdxfrw/drw_base.h
    ${SRC_DIR}/libdxfrw/drw_classes.h
    ${SRC_DIR}/libdxfrw/drw_entities.h
    ${SRC_DIR}/libdxfrw/drw_header.h
    ${SRC_DIR}/libdxfrw/drw_interface.h
    ${SRC_DIR}/libdxfrw/drw_objects.h
    ${SRC_DIR}/libdxfrw/dx_data.h
    ${SRC_DIR}/libdxfrw/dx_iface.h
    ${SRC_DIR}/libdxfrw/intern/drw_cptable932.h
    ${SRC_DIR}/libdxfrw/intern/drw_cptable936.h
    ${SRC_DIR}/libdxfrw/intern/drw_cptable949.h
    ${SRC_DIR}/libdxfrw/intern/drw_cptable950.h
    ${SRC_DIR}/libdxfrw/intern/drw_cptables.h
    ${SRC_DIR}/libdxfrw/intern/drw_dbg.h
    ${SRC_DIR}/libdxfrw/intern/drw_textcodec.h
    ${SRC_DIR}/libdxfrw/intern/dwgbuffer.h
    ${SRC_DIR}/libdxfrw/intern/dwgreader.h
    ${SRC_DIR}/libdxfrw/intern/dwgreader15.h
    ${SRC_DIR}/libdxfrw/intern/dwgreader18.h
    ${SRC_DIR}/libdxfrw/intern/dwgreader21.h
    ${SRC_DIR}/libdxfrw/intern/dwgreader24.h
    ${SRC_DIR}/libdxfrw/intern/dwgreader27.h
    ${SRC_DIR}/libdxfrw/intern/dwgutil.h
    ${SRC_DIR}/libdxfrw/intern/dxfreader.h
    ${SRC_DIR}/libdxfrw/intern/dxfwriter.h
    ${SRC_DIR}/libdxfrw/intern/rscodec.h
    ${SRC_DIR}/libdxfrw/libdwgr.h
    ${SRC_DIR}/libdxfrw/libdxfrw.h
    ${SRC_DIR}/libdxfrw/libdxfrw_functions.h
    ${SRC_DIR}/libdxfrw/main_doc.h
    ${SRC_DIR}/libgcode/gcode_program.h
    ${SRC_DIR}/libgcode/interface.h
    ${SRC_DIR}/libgcode/parser.h
    ${SRC_DIR}/libocct/draw_primitives.h
    ${SRC_DIR}/libocct/opencascade.h
    ${SRC_DIR}/libskynet/char_to_string.h
    ${SRC_DIR}/libskynet/directory.h
    ${SRC_DIR}/libspline/bezier_spline.h
    ${SRC_DIR}/libspline/cubic_spline.h
    ${SRC_DIR}/libspline/spline.h
    ${SRC_DIR}/libdxfrw/drw_classes.cpp
    ${SRC_DIR}/libdxfrw/drw_entities.cpp
    ${SRC_DIR}/libdxfrw/drw_header.cpp
    ${SRC_DIR}/libdxfrw/drw_objects.cpp
    ${SRC_DIR}/libdxfrw/dx_iface.cpp
    ${SRC_DIR}/libdxfrw/intern/drw_dbg.cpp
    ${SRC_DIR}/libdxfrw/intern/drw_textcodec.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgbuffer.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgreader.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgreader15.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgreader18.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgreader21.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgreader24.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgreader27.cpp
    ${SRC_DIR}/libdxfrw/intern/dwgutil.cpp
    ${SRC_DIR}/libdxfrw/intern/dxfreader.cpp
    ${SRC_DIR}/libdxfrw/intern/dxfwriter.cpp
    ${SRC_DIR}/libdxfrw/intern/rscodec.cpp
    ${SRC_DIR}/libdxfrw/libdwgr.cpp
    ${SRC_DIR}/libdxfrw/libdxfrw.cpp
    ${SRC_DIR}/libdxfrw/libdxfrw_functions.cpp
    ${SRC_DIR}/libgcode/gcode_program.cpp
    ${SRC_DIR}/libgcode/interface.cpp
    ${SRC_DIR}/libgcode/parser.cpp
    ${SRC_DIR}/libocct/draw_primitives.cpp
    ${SRC_DIR}/libocct/opencascade.cpp
    ${SRC_DIR}/libskynet/char_to_string.cpp
    ${SRC_DIR}/libskynet/directory.cpp
    ${SRC_DIR}/libspline/bezier_spline.cpp
    ${SRC_DIR}/libspline/cubic_spline.cpp
    ${SRC_DIR}/libspline/spline.cpp
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

target_include_directories(${PROJECT_NAME} PRIVATE "${SRC_DIR}/build/src/hal/halcore/${RT_CORE}")
#target_include_directories(${PROJECT_NAME} PRIVATE "${SRC_DIR}/../../halcore/${RT_CORE}")

# Link to other libraries /opt/hal-core-2.0/build/src/hal/halcore
target_include_directories(${PROJECT_NAME} PRIVATE "${SRC_DIR}/build/src/hal/${RT_CORE}")
target_link_libraries(${PROJECT_NAME} PRIVATE pthread rt ${RT_CORE} ${OpenCASCADE_LIBS} Qt5::Widgets Qt5::OpenGL ${OPENGL_LIBRARIES} ${GLUT_LIBRARY})

# make install
install(TARGETS ${PROJECT} DESTINATION ${RT_DIR})
