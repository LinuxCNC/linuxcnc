cmake_minimum_required(VERSION 3.11)

include(googlebenchmark.cmake)
include(clipper.cmake)

macro(add_benchmark name)
  add_executable(${name} ${name}.cpp)
  target_link_libraries(${name}
    PRIVATE ${CAVC_CPP_HEADER_ONLY_LIB})
  target_link_libraries(${name}
    PRIVATE PolylineFactory)
  target_link_libraries(${name}
    PRIVATE benchmark::benchmark)
	if (MSVC)
      target_link_options(${name} PRIVATE $<$<CONFIG:RELWITHDEBINFO>:/PROFILE>)
    endif()
endmacro()

add_benchmark(offsetbenchmarks)
add_benchmark(spatialindexbenchmarks)
add_benchmark(extentsbenchmarks)
add_benchmark(areabenchmarks)
add_benchmark(pathlengthbenchmarks)
add_benchmark(windingnumberbenchmarks)
add_benchmark(combinebenchmarks)

add_benchmark(clipperbenchmarks)
target_link_libraries(clipperbenchmarks
  PRIVATE clipper_static)
