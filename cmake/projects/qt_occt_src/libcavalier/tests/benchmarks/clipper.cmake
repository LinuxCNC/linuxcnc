include(FetchContent)
FetchContent_Declare(
    clipper_static
    GIT_REPOSITORY       https://github.com/jbuckmccready/clipper-lib
    GIT_TAG              origin/master
)

fetchcontent_getproperties(clipper_static)
if(NOT clipper_static_POPULATED)
  fetchcontent_populate(clipper_static)
  add_subdirectory(${clipper_static_SOURCE_DIR} ${clipper_static_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()
