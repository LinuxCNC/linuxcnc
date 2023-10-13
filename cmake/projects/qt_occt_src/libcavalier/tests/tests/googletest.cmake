include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY       https://github.com/google/googletest.git
    GIT_TAG              origin/master
)

fetchcontent_getproperties(googletest)
if(NOT googletest_POPULATED)
  # this is required for msvc or we get linker errors
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  fetchcontent_populate(googletest)
  add_subdirectory(${googletest_SOURCE_DIR} ${googletest_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()
