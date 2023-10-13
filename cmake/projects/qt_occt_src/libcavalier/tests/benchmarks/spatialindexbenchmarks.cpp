#include "benchmarkprofiles.h"
#include "cavc/staticspatialindex.hpp"
#include <benchmark/benchmark.h>

static void createIndex(NoSetup, TestProfile const &profile) {
  cavc::createApproxSpatialIndex(profile.pline);
}

CAVC_CREATE_BENCHMARKS(createIndex, NoSetup, createIndex, benchmark::kMicrosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(createIndex, NoSetup, createIndex, 0.01, benchmark::kMicrosecond)

struct QuerySetup {
  std::vector<std::size_t> queryResults;
  std::vector<std::size_t> queryStack;
  cavc::StaticSpatialIndex<double> spatialIndex;
  QuerySetup(TestProfile const &profile)
      : spatialIndex(cavc::createApproxSpatialIndex(profile.pline)) {}
};

static void queryIndexReuseStack(QuerySetup &setup, TestProfile const &profile) {
  profile.pline.visitSegIndices([&](std::size_t i, std::size_t j) {
    cavc::AABB<double> bb = cavc::createFastApproxBoundingBox(profile.pline[i], profile.pline[j]);
    setup.queryResults.clear();
    bb.expand(0.1);
    setup.spatialIndex.query(bb.xMin, bb.yMin, bb.xMax, bb.yMax, setup.queryResults,
                             setup.queryStack);
    return true;
  });
}

CAVC_CREATE_BENCHMARKS(queryIndexReuseStack, QuerySetup, queryIndexReuseStack,
                       benchmark::kMicrosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(queryIndexReuseStack, QuerySetup, queryIndexReuseStack, 0.01,
                               benchmark::kMicrosecond)

BENCHMARK_MAIN();
