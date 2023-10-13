## FindPkgConfig.cmake
## by  Albert Strasheim <http://students . ee . sun . ac . za/~albert/>
## and Alex Brooks (a.brooks at acfr . usyd . edu . au)
##
## This module finds packages using pkg-config, which retrieves
## information about packages from special metadata files.
##
## See http://www . freedesktop . org/Software/pkgconfig/
##
## -------------------------------------------------------------------
##
## Usage:
##
## INCLUDE( ${CMAKE_ROOT}/Modules/FindPkgConfig.cmake)
## 
## IF ( CMAKE_PKGCONFIG_EXECUTABLE )
##
##     # Find all the librtk stuff with pkg-config
##     PKGCONFIG( "librtk >= 2.0" HAVE_RTK RTK_INCLUDE_DIRS RTK_DEFINES RTK_LINK_DIRS RTK_LIBS )
##
## ELSE  ( CMAKE_PKGCONFIG_EXECUTABLE )
##
##     # Can't find pkg-config -- have to find librtk somehow else
##
## ENDIF ( CMAKE_PKGCONFIG_EXECUTABLE )
##
##
## Notes:
## 
## You can set the PKG_CONFIG_PATH environment variable to tell
## pkg-config where to search for .pc files. See pkg-config(1) for
## more information.
##
#
# FIXME: IF(WIN32) pkg-config --msvc-syntax ENDIF(WIN32) ???
#
# FIXME: Parsing of pkg-config output is specific to gnu-style flags
#

FIND_PROGRAM(CMAKE_PKGCONFIG_EXECUTABLE pkg-config)
MARK_AS_ADVANCED(CMAKE_PKGCONFIG_EXECUTABLE)

########################################

MACRO(PKGCONFIG_PARSE_FLAGS FLAGS INCLUDES DEFINES)

  #MESSAGE("DEBUG: FLAGS: ${FLAGS}")

  STRING(REGEX MATCHALL " -I[^ ]*" ${INCLUDES} "${FLAGS}")
  STRING(REGEX REPLACE " -I" "" ${INCLUDES} "${${INCLUDES}}")
  #MESSAGE("DEBUG: INCLUDES: ${${INCLUDES}}")
  
  STRING(REGEX REPLACE " -I[^ ]*" "" ${DEFINES} "${FLAGS}")
  #MESSAGE("DEBUG: DEFINES: ${${DEFINES}}")

ENDMACRO(PKGCONFIG_PARSE_FLAGS)

########################################

MACRO(PKGCONFIG_PARSE_LIBS LIBS LINKDIRS LINKLIBS)

  #MESSAGE("DEBUG: LIBS: ${LIBS}")

  STRING(REGEX MATCHALL " -L[^ ]*" ${LINKDIRS} "${LIBS}")
  STRING(REGEX REPLACE " -L" "" ${LINKDIRS} "${${LINKDIRS}}")
  #MESSAGE("DEBUG: LINKDIRS: ${${LINKDIRS}}")

  STRING(REGEX MATCHALL " -l[^ ]*" ${LINKLIBS} "${LIBS}")
  STRING(REGEX REPLACE " -l" "" ${LINKLIBS} "${${LINKLIBS}}")
  #MESSAGE("DEBUG: LINKLIBS: ${${LINKLIBS}}")

ENDMACRO(PKGCONFIG_PARSE_LIBS)

########################################

MACRO(PKGCONFIG LIBRARY FOUND INCLUDE_DIRS DEFINES LINKDIRS LINKLIBS)

  SET(${FOUND} 0)

# alexm: why print it twice? once here, and once when it's found/not found
#   MESSAGE("-- Looking for ${LIBRARY}")
  
  IF(CMAKE_PKGCONFIG_EXECUTABLE)
    # MESSAGE("DEBUG: pkg-config executable found")
    
    EXEC_PROGRAM(${CMAKE_PKGCONFIG_EXECUTABLE}
      ARGS "'${LIBRARY}'"
      OUTPUT_VARIABLE PKGCONFIG_OUTPUT
      RETURN_VALUE PKGCONFIG_RETURN)

    IF(NOT PKGCONFIG_RETURN)
      
      # set C_FLAGS and CXX_FLAGS
      EXEC_PROGRAM(${CMAKE_PKGCONFIG_EXECUTABLE}
        ARGS "--cflags '${LIBRARY}'"
        OUTPUT_VARIABLE CMAKE_PKGCONFIG_C_FLAGS)

      #SET(CMAKE_PKGCONFIG_CXX_FLAGS "${CMAKE_PKGCONFIG_C_FLAGS}")
      PKGCONFIG_PARSE_FLAGS(" ${CMAKE_PKGCONFIG_C_FLAGS}" ${INCLUDE_DIRS} ${DEFINES} )
      
      # set LIBRARIES
      EXEC_PROGRAM(${CMAKE_PKGCONFIG_EXECUTABLE}
        ARGS "--libs '${LIBRARY}'"
        OUTPUT_VARIABLE CMAKE_PKGCONFIG_LIBRARIES)
      PKGCONFIG_PARSE_LIBS (" ${CMAKE_PKGCONFIG_LIBRARIES}" ${LINKDIRS} ${LINKLIBS} )

      SET(${FOUND} 1)
      MESSAGE("-- Looking for ${LIBRARY} -- found")

    ELSE(NOT PKGCONFIG_RETURN)
      MESSAGE("-- Looking for ${LIBRARY} -- not found")
      
      SET(CMAKE_PKGCONFIG_C_FLAGS "")
      SET(CMAKE_PKGCONFIG_CXX_FLAGS "")
      SET(CMAKE_PKGCONFIG_LIBRARIES "")
      SET(${INCLUDE_DIRS} "")
      SET(${DEFINES} "")
      SET(${LINKDIRS} "")
      SET(${LINKLIBS} "")

    ENDIF(NOT PKGCONFIG_RETURN)

  ELSE(CMAKE_PKGCONFIG_EXECUTABLE)
    MESSAGE("-- pkg-config executable NOT FOUND")
  ENDIF(CMAKE_PKGCONFIG_EXECUTABLE)

  #MESSAGE("Have  ${LIBRARY}       : ${${FOUND}}")
  #MESSAGE("${LIBRARY} include dirs: ${${INCLUDE_DIRS}}")
  #MESSAGE("${LIBRARY} defines     : ${${DEFINES}}")
  #MESSAGE("${LIBRARY} link dirs   : ${${LINKDIRS}}")
  #MESSAGE("${LIBRARY} link libs   : ${${LINKLIBS}}")

ENDMACRO(PKGCONFIG)
