#include "benchmarkprofiles.h"
#include "cavc/polylineoffset.hpp"
#include <benchmark/benchmark.h>

const double arcError = 0.01;

static void offset(NoSetup, TestProfile const &profile) {
  for (std::size_t i = 1; i <= profile.offsetCount; ++i) {
    double offset = i * profile.offsetDelta;
    cavc::parallelOffset(profile.pline, offset);
    cavc::parallelOffset(profile.pline, -offset);
  }
}

CAVC_CREATE_BENCHMARKS(offset, NoSetup, offset, benchmark::kMillisecond)

CAVC_CREATE_NO_ARCS_BENCHMARKS(offset, NoSetup, offset, arcError, benchmark::kMillisecond)

BENCHMARK_MAIN();
