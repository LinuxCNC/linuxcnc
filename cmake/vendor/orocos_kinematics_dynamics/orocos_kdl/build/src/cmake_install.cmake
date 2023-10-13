# Install script for directory: /home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so.1.4.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so.1.4"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "$ORIGIN/../lib")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/build/src/liborocos-kdl.so.1.4.0"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/build/src/liborocos-kdl.so.1.4"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so.1.4.0"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so.1.4"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHANGE
           FILE "${file}"
           OLD_RPATH "::::::::::::::"
           NEW_RPATH "$ORIGIN/../lib")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so")
    file(RPATH_CHECK
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so"
         RPATH "$ORIGIN/../lib")
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/build/src/liborocos-kdl.so")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so")
    file(RPATH_CHANGE
         FILE "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so"
         OLD_RPATH "::::::::::::::"
         NEW_RPATH "$ORIGIN/../lib")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "/usr/bin/strip" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/liborocos-kdl.so")
    endif()
  endif()
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/kdl" TYPE FILE FILES
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/articulatedbodyinertia.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chain.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chaindynparam.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainfdsolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainfdsolver_recursive_newton_euler.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainfksolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainfksolverpos_recursive.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainfksolvervel_recursive.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainidsolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainidsolver_recursive_newton_euler.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainidsolver_vereshchagin.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolverpos_lma.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolverpos_nr.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolverpos_nr_jl.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolvervel_pinv.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolvervel_pinv_givens.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolvervel_pinv_nso.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainiksolvervel_wdls.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainjnttojacdotsolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/chainjnttojacsolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/frameacc.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/frameacc.inl"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/frameacc_io.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/frames.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/frames.inl"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/frames_io.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/framevel.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/framevel.inl"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/framevel_io.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/jacobian.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/jntarray.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/jntarrayacc.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/jntarrayvel.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/jntspaceinertiamatrix.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/joint.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/kdl.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/kinfam.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/kinfam_io.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/motion.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/path.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/path_circle.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/path_composite.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/path_cyclic_closed.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/path_line.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/path_point.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/path_roundedcomposite.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/rigidbodyinertia.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/rotational_interpolation.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/rotational_interpolation_sa.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/rotationalinertia.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/segment.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/solveri.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/stiffness.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/trajectory.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/trajectory_composite.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/trajectory_segment.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/trajectory_stationary.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/tree.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treefksolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treefksolverpos_recursive.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treeidsolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treeidsolver_recursive_newton_euler.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treeiksolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treeiksolverpos_nr_jl.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treeiksolverpos_online.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treeiksolvervel_wdls.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/treejnttojacsolver.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/velocityprofile.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/velocityprofile_dirac.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/velocityprofile_rect.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/velocityprofile_spline.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/velocityprofile_trap.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/velocityprofile_traphalf.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/build/src/config.h"
    )
endif()

if("x${CMAKE_INSTALL_COMPONENT}x" STREQUAL "xUnspecifiedx" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/kdl/utilities" TYPE FILE FILES
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/error.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/error_stack.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/kdl-config.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/ldl_solver_eigen.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/rall1d.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/rall1d_io.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/rall2d.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/rall2d_io.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/rallNd.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/scoped_ptr.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/svd_HH.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/svd_eigen_HH.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/svd_eigen_Macie.hpp"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/traits.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/utility.h"
    "/home/user/linuxcnc/cmake/vendor/orocos_kinematics_dynamics/orocos_kdl/src/utilities/utility_io.h"
    )
endif()

