#include "benchmarkprofiles.h"
#include "cavc/polyline.hpp"
#include <benchmark/benchmark.h>

static void area(NoSetup, TestProfile const &profile) {
  cavc::getArea(profile.pline);
}
CAVC_CREATE_BENCHMARKS(area, NoSetup,area, benchmark::kNanosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(area, NoSetup, area, 0.01, benchmark::kNanosecond)

BENCHMARK_MAIN();
