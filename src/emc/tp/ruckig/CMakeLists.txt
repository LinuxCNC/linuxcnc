cmake_minimum_required(VERSION 3.15)


project(ruckig VERSION 0.15.5 LANGUAGES CXX)


include(GNUInstallDirs)


option(BUILD_EXAMPLES "Build example programs" ON)
option(BUILD_PYTHON_MODULE "Build Python wrapper with pybind11" OFF)
option(BUILD_CLOUD_CLIENT "Build cloud client to calculate Ruckig Pro trajectories remotely" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_BENCHMARK "Build benchmark" OFF)
option(BUILD_SHARED_LIBS "Build as shared library" ON)

if(WIN32 AND BUILD_SHARED_LIBS)
  option(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS "On Windows, export all symbols when building a shared library." ON)
endif()


add_library(ruckig
  src/ruckig/brake.cpp
  src/ruckig/position_first_step1.cpp
  src/ruckig/position_first_step2.cpp
  src/ruckig/position_second_step1.cpp
  src/ruckig/position_second_step2.cpp
  src/ruckig/position_third_step1.cpp
  src/ruckig/position_third_step2.cpp
  src/ruckig/velocity_second_step1.cpp
  src/ruckig/velocity_second_step2.cpp
  src/ruckig/velocity_third_step1.cpp
  src/ruckig/velocity_third_step2.cpp
)

target_compile_features(ruckig PUBLIC cxx_std_11)
if(MSVC)
  target_compile_options(ruckig PRIVATE /W4)
else()
  target_compile_options(ruckig PRIVATE -Wall -Wextra -Wunused-parameter)
endif()
target_include_directories(ruckig PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(ruckig PUBLIC)

if(MSVC)
  target_compile_definitions(ruckig PUBLIC _USE_MATH_DEFINES)
endif()

if(BUILD_CLOUD_CLIENT)
  target_sources(ruckig PRIVATE src/ruckig/cloud_client.cpp)
  target_include_directories(ruckig PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/ruckig/third_party>
  )
  target_compile_definitions(ruckig PUBLIC WITH_CLOUD_CLIENT)
endif()

add_library(ruckig::ruckig ALIAS ruckig)


if(BUILD_TESTS)
  enable_testing()

  add_library(test-dependencies INTERFACE)
  target_link_libraries(test-dependencies INTERFACE ruckig)
  target_include_directories(test-dependencies INTERFACE third_party)
  if(MSVC)
    target_compile_options(test-dependencies INTERFACE /W4)
  else()
    target_compile_options(test-dependencies INTERFACE -Wall -Wextra)
  endif()

  add_executable(test-target test/test_target.cpp)
  target_link_libraries(test-target PRIVATE test-dependencies)
  add_test(NAME test-target COMMAND test-target)
  if(NOT MSVC)
    include(CTest)
    find_program(MEMORYCHECK_COMMAND valgrind)
    set(MEMORYCHECK_COMMAND_OPTIONS "--tool=memcheck --leak-check=full --trace-children=yes \
      --track-origins=yes  --keep-debuginfo=yes --error-exitcode=100")
  endif()
endif()


if(BUILD_BENCHMARK)
  add_executable(benchmark-target test/benchmark_target.cpp)

  target_link_libraries(benchmark-target PRIVATE ruckig)
endif()


if(BUILD_EXAMPLES)
  set(EXAMPLES_LIST 01_position 02_position_offline 05_velocity 06_stop 07_minimum_duration 09_dynamic_dofs 12_custom_vector_type 13_custom_vector_type_dynamic_dofs)
  if(TARGET Eigen3::Eigen)
    list(APPEND EXAMPLES_LIST 11_eigen_vector_type)
  endif()

  if(BUILD_CLOUD_CLIENT)
    list(APPEND EXAMPLES_LIST 03_waypoints 04_waypoints_online 08_per_section_minimum_duration 10_dynamic_dofs_waypoints)
  endif()

  foreach(example IN LISTS EXAMPLES_LIST)
    add_executable(example-${example} examples/${example}.cpp)
    target_link_libraries(example-${example} PRIVATE ruckig)
  endforeach()
endif()


if(BUILD_PYTHON_MODULE)
  find_package(Python 3.8 REQUIRED COMPONENTS Interpreter Development)
  find_package(pybind11 REQUIRED)

  pybind11_add_module(python_ruckig src/ruckig/python.cpp)
  target_compile_features(python_ruckig PUBLIC cxx_std_11)
  target_link_libraries(python_ruckig PUBLIC ruckig)
  if(BUILD_CLOUD_CLIENT)
    target_compile_definitions(python_ruckig PUBLIC WITH_CLOUD_CLIENT)
  endif()

  set_target_properties(python_ruckig PROPERTIES OUTPUT_NAME ruckig)
  set_target_properties(python_ruckig PROPERTIES ARCHIVE_OUTPUT_NAME python_ruckig)

  install(TARGETS python_ruckig LIBRARY DESTINATION .)
endif()


# Add support for installation
include(CMakePackageConfigHelpers)

# Install headers
install(DIRECTORY include/ruckig DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(BUILD_CLOUD_CLIENT)
  install(DIRECTORY third_party/httplib third_party/nlohmann DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ruckig/third_party)
endif()

# Install library
install(TARGETS ruckig
  EXPORT ${PROJECT_NAME}-targets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install CMake config files
set(ruckig_INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME})

install(EXPORT ${PROJECT_NAME}-targets
  DESTINATION ${ruckig_INSTALL_CONFIGDIR}
  NAMESPACE ruckig::
)

configure_package_config_file(cmake/ruckig-config.cmake.in ruckig-config.cmake
  INSTALL_DESTINATION ${ruckig_INSTALL_CONFIGDIR}
)

write_basic_package_version_file(ruckig-config-version.cmake
  VERSION ${ruckig_VERSION}
  COMPATIBILITY AnyNewerVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/ruckig-config.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/ruckig-config-version.cmake"
  DESTINATION ${ruckig_INSTALL_CONFIGDIR}
)

install(FILES
  "${CMAKE_CURRENT_SOURCE_DIR}/package.xml"
  DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}
)


# Enable Packaging
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_CONTACT "Lars Berscheid (lars.berscheid@ruckig.com)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Motion Generation for Robots and Machines.")
set(CPACK_PACKAGE_VENDOR "Lars Berscheid")
set(CPACK_PACKAGE_VERSION ${ruckig_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_SYSTEM_NAME ${CMAKE_HOST_SYSTEM_PROCESSOR})
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://ruckig.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Lars Berscheid")
set(CPACK_DEBIAN_PACKAGE_VERSION ${CPACK_PACKAGE_VERSION}-1)
set(CPACK_DEBIAN_PACKAGE_CONFLICTS "ros-melodic-ruckig, ros-noetic-ruckig, ros-foxy-ruckig, ros-galactic-ruckig, ros-humble-ruckig, ros-rolling-ruckig")
include(CPack)
