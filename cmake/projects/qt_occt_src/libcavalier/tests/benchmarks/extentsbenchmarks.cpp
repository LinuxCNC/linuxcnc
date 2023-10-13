#include "benchmarkprofiles.h"
#include "cavc/polyline.hpp"
#include <benchmark/benchmark.h>

static void extents(NoSetup, TestProfile const &profile) {
  cavc::getExtents(profile.pline);
}
CAVC_CREATE_BENCHMARKS(extents, NoSetup, extents, benchmark::kNanosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(extents, NoSetup, extents, 0.01, benchmark::kNanosecond)

BENCHMARK_MAIN();
