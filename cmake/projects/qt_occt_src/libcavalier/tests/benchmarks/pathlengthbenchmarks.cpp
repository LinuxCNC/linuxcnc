#include "benchmarkprofiles.h"
#include "cavc/polyline.hpp"
#include <benchmark/benchmark.h>

static void pathLength(NoSetup, TestProfile const &profile) {
  cavc::getPathLength(profile.pline);
}
CAVC_CREATE_BENCHMARKS(pathLength, NoSetup, pathLength, benchmark::kNanosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(pathLength, NoSetup, pathLength, 0.01, benchmark::kNanosecond)

BENCHMARK_MAIN();
