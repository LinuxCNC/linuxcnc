#include "benchmarkprofiles.h"
#include "polyline.hpp"
#include <benchmark/benchmark.h>

struct WindingNumberSetup {
  std::vector<cavc::Vector2<double>> testPts;
  WindingNumberSetup(TestProfile const &profile) {
    auto extents = cavc::getExtents(profile.pline);
    // expand out all directions by half the polyline width for some of the test points to for sure
    // be outside the polyline
    extents.expand((extents.xMax - extents.xMin) / 2.0);
    double width = extents.xMax - extents.xMin;
    double height = extents.yMax - extents.yMin;

    std::size_t gridDim = 10;
    testPts.reserve(gridDim * gridDim);
    // grid is inclusive at ends so we go from 0 to gridDim - 1
    for (std::size_t i = 0; i < gridDim; ++i) {
      for (std::size_t j = 0; j < gridDim; ++j) {
        // scale by gridDim - 1 (max iteration value)
        double x = static_cast<double>(i) / (gridDim - 1) * width + extents.xMin;
        double y = static_cast<double>(j) / (gridDim - 1) * height + extents.yMin;
        testPts.emplace_back(x, y);
      }
    }
  }
};

static void windingNumber(WindingNumberSetup const &setup, TestProfile const &profile) {
  for (auto const &pt : setup.testPts) {
    cavc::getWindingNumber(profile.pline, pt);
  }
}

CAVC_CREATE_BENCHMARKS(windingNumber100PtGrid, WindingNumberSetup, windingNumber,
                       benchmark::kMicrosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(windingNumber100PtGrid, WindingNumberSetup, windingNumber, 0.01,
                               benchmark::kMicrosecond)

BENCHMARK_MAIN();
