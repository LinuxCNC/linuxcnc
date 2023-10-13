cmake_minimum_required(VERSION 3.0)
set(PROJECT "libnml") 
project(${PROJECT} VERSION 0 LANGUAGES CXX)

# Set variables.
set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}")

# Set includepaths.
include_directories(${SRC_DIR})
include_directories(${SRC_DIR}/../../src)
include_directories(${SRC_DIR}/../../src/rtapi)
include_directories(${SRC_DIR}/../../src/emc)
include_directories(${SRC_DIR}/../../src/libnml/rcs)
include_directories(${SRC_DIR}/../../src/libnml/os_intf)
include_directories(${SRC_DIR}/../../src/libnml/buffer)
include_directories(${SRC_DIR}/../../src/libnml/cms)
include_directories(${SRC_DIR}/../../src/libnml/nml)
include_directories(${SRC_DIR}/../../src/libnml/linklist)
include_directories(${SRC_DIR}/../../src/libnml/posemath)
include_directories(${SRC_DIR}/../../src/libnml/inifile)

# Set prefix.
set(CMAKE_SHARED_LIBRARY_PREFIX "")

ADD_LIBRARY(${PROJECT} SHARED 
    ${SRC_DIR}/../../src/libnml/rcs/rcs_print.cc
    ${SRC_DIR}/../../src/libnml/rcs/rcs_exit.cc
    ${SRC_DIR}/../../src/libnml/os_intf/_sem.c
    ${SRC_DIR}/../../src/libnml/os_intf/_shm.c
    ${SRC_DIR}/../../src/libnml/os_intf/_timer.c
    ${SRC_DIR}/../../src/libnml/os_intf/sem.cc
    ${SRC_DIR}/../../src/libnml/os_intf/shm.cc
    ${SRC_DIR}/../../src/libnml/os_intf/timer.cc
    ${SRC_DIR}/../../src/libnml/buffer/locmem.cc
    ${SRC_DIR}/../../src/libnml/buffer/memsem.cc
    ${SRC_DIR}/../../src/libnml/buffer/phantom.cc
    ${SRC_DIR}/../../src/libnml/buffer/physmem.cc
    ${SRC_DIR}/../../src/libnml/buffer/recvn.c
    ${SRC_DIR}/../../src/libnml/buffer/sendn.c
    ${SRC_DIR}/../../src/libnml/buffer/shmem.cc
    ${SRC_DIR}/../../src/libnml/buffer/tcpmem.cc
    ${SRC_DIR}/../../src/libnml/cms/cms.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_aup.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_cfg.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_in.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_dup.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_pm.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_srv.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_up.cc
    ${SRC_DIR}/../../src/libnml/cms/cms_xup.cc
    ${SRC_DIR}/../../src/libnml/cms/cmsdiag.cc
    ${SRC_DIR}/../../src/libnml/cms/tcp_opts.cc
    ${SRC_DIR}/../../src/libnml/cms/tcp_srv.cc
    ${SRC_DIR}/../../src/libnml/nml/cmd_msg.cc
    ${SRC_DIR}/../../src/libnml/nml/nml_oi.cc
    ${SRC_DIR}/../../src/libnml/nml/nml_srv.cc
    ${SRC_DIR}/../../src/libnml/nml/nml.cc
    ${SRC_DIR}/../../src/libnml/nml/nmldiag.cc
    ${SRC_DIR}/../../src/libnml/nml/nmlmsg.cc
    ${SRC_DIR}/../../src/libnml/nml/stat_msg.cc
    ${SRC_DIR}/../../src/libnml/linklist/linklist.cc
)

add_definitions(-DULAPI)

add_library(linuxcnchal SHARED IMPORTED) # or STATIC instead of SHARED
set_target_properties(linuxcnchal PROPERTIES
  IMPORTED_LOCATION "${SRC_DIR}/../build/liblinuxcnchal/liblinuxcnchal.so"
  INTERFACE_INCLUDE_DIRECTORIES "${SRC_DIR}/../liblinuxcnchal/"
)

# Link to libraries.
target_link_libraries(${PROJECT_NAME} PRIVATE linuxcnchal -fPIC)

# Set custom output name :
# set_property(TARGET  ${PROJECT} PROPERTY OUTPUT_NAME thename)

# Add verion nr. .so.0.0.0
set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})

# Make install
install(TARGETS ${PROJECT} DESTINATION ${BUILD_DIR}/../../../lib)














