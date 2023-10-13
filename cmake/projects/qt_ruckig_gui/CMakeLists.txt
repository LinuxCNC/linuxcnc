cmake_minimum_required(VERSION 3.5)
set(PROJECT "qt_ruckig_gui")
project(${PROJECT} LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# QtCreator supports the following variables for Android, which are identical to qmake Android variables.
# Check https://doc.qt.io/qt/deployment-android.html for more information.
# They need to be set before the find_package( ...) calls below.

#if(ANDROID)
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
#    if (ANDROID_ABI STREQUAL "armeabi-v7a")
#        set(ANDROID_EXTRA_LIBS
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libcrypto.so
#            ${CMAKE_CURRENT_SOURCE_DIR}/path/to/libssl.so)
#    endif()
#endif()

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths for header files etc.
include_directories(${SRC_DIR}/../)
include_directories(${SRC_DIR}/../../)
include_directories(${SRC_DIR}/../../vendor/ruckig)
include_directories(${SRC_DIR}/../../components/sc_tp_src)
include_directories(${SRC_DIR}/../../../src)

set(PROJECT_SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/mainwindow.cpp
    ${SRC_DIR}/mainwindow.h
    ${SRC_DIR}/mainwindow.ui
    ${SRC_DIR}/opengl.h
    ${SRC_DIR}/opengl.cpp
    ${SRC_DIR}/../../vendor/ruckig/block.hpp
    ${SRC_DIR}/../../vendor/ruckig/brake.hpp
    ${SRC_DIR}/../../vendor/ruckig/brake.cpp
    ${SRC_DIR}/../../vendor/ruckig/calculator.hpp
    ${SRC_DIR}/../../vendor/ruckig/calculator_online.hpp
    ${SRC_DIR}/../../vendor/ruckig/calculator_target.hpp
    ${SRC_DIR}/../../vendor/ruckig/input_parameter.hpp
    ${SRC_DIR}/../../vendor/ruckig/output_parameter.hpp
    ${SRC_DIR}/../../vendor/ruckig/position.hpp
    ${SRC_DIR}/../../vendor/ruckig/position-step1.cpp
    ${SRC_DIR}/../../vendor/ruckig/position-step2.cpp
    ${SRC_DIR}/../../vendor/ruckig/profile.hpp
    ${SRC_DIR}/../../vendor/ruckig/reflexxes_comparison.hpp
    ${SRC_DIR}/../../vendor/ruckig/roots.hpp
    ${SRC_DIR}/../../vendor/ruckig/ruckig.hpp
    ${SRC_DIR}/../../vendor/ruckig/trajectory.hpp
    ${SRC_DIR}/../../vendor/ruckig/utils.hpp
    ${SRC_DIR}/../../vendor/ruckig/velocity.hpp
    ${SRC_DIR}/../../vendor/ruckig/velocity-step1.cpp
    ${SRC_DIR}/../../vendor/ruckig/velocity-step2.cpp
    ${SRC_DIR}/../../components/sc_tp_src/sc_ruckig.h
    ${SRC_DIR}/../../components/sc_tp_src/sc_ruckig.cpp
    )

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(qt_ruckig_gui
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

target_link_libraries(${PROJECT} PRIVATE Qt${QT_VERSION_MAJOR}::Widgets -lGL pthread rt)

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/bin)

# Notes
