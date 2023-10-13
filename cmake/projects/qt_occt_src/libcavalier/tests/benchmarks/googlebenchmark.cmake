include(FetchContent)
FetchContent_Declare(
    benchmark
    GIT_REPOSITORY       https://github.com/google/benchmark.git
    GIT_TAG              origin/master
)

fetchcontent_getproperties(benchmark)
if(NOT benchmark_POPULATED)
  set(BENCHMARK_ENABLE_TESTING OFF
      CACHE BOOL "Enable testing of the benchmark library."
      FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF
      CACHE BOOL "Enable building the unit tests which depend on gtest"
      FORCE)
  fetchcontent_populate(benchmark)
  add_subdirectory(${benchmark_SOURCE_DIR} ${benchmark_BINARY_DIR}
                   EXCLUDE_FROM_ALL)
endif()
