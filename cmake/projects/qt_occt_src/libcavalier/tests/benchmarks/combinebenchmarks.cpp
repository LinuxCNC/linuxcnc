#include "benchmarkprofiles.h"
#include "cavc/polylinecombine.hpp"
#include <benchmark/benchmark.h>

struct CombineShiftedSetup {
  std::vector<cavc::Polyline<double>> shiftedProfiles;

  CombineShiftedSetup(TestProfile const &profile) {
    auto extents = cavc::getExtents(profile.pline);
    double halfWidth = (extents.xMax - extents.xMin) / 2.0;
    double halfHeight = (extents.yMax - extents.yMin) / 2.0;

    auto createShifted = [&](double angle) {
      auto shifted = profile.pline;
      double xOffset = halfWidth * std::cos(angle);
      double yOffset = halfHeight * std::sin(angle);
      cavc::translatePolyline(shifted, {xOffset, yOffset});
      return shifted;
    };

    std::size_t shiftedCount = 16;
    shiftedProfiles.reserve(shiftedCount);
    for (std::size_t i = 0; i < shiftedCount; ++i) {
      double angle = static_cast<double>(i) / shiftedCount * cavc::utils::tau<double>();
      shiftedProfiles.push_back(createShifted(angle));
    }
  }
};

static void combineShifted(CombineShiftedSetup const &setup, TestProfile const &profile) {
  for (auto const &shifted : setup.shiftedProfiles) {
    cavc::combinePolylines(profile.pline, shifted, cavc::PlineCombineMode::Union);
    cavc::combinePolylines(profile.pline, shifted, cavc::PlineCombineMode::Exclude);
    cavc::combinePolylines(profile.pline, shifted, cavc::PlineCombineMode::Intersect);
    cavc::combinePolylines(profile.pline, shifted, cavc::PlineCombineMode::XOR);
  }
}

CAVC_CREATE_BENCHMARKS(combine16Shifted, CombineShiftedSetup, combineShifted,
                       benchmark::kMicrosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(combine16Shifted, CombineShiftedSetup, combineShifted, 0.01,
                               benchmark::kMicrosecond)

static void combineCoincident(NoSetup, TestProfile const &profile) {
  cavc::combinePolylines(profile.pline, profile.pline, cavc::PlineCombineMode::Union);
  cavc::combinePolylines(profile.pline, profile.pline, cavc::PlineCombineMode::Exclude);
  cavc::combinePolylines(profile.pline, profile.pline, cavc::PlineCombineMode::Intersect);
  cavc::combinePolylines(profile.pline, profile.pline, cavc::PlineCombineMode::XOR);
}

CAVC_CREATE_BENCHMARKS(combineCoincident, NoSetup, combineCoincident, benchmark::kMicrosecond)
CAVC_CREATE_NO_ARCS_BENCHMARKS(combineCoincident, NoSetup, combineCoincident, 0.01,
                               benchmark::kMicrosecond)

BENCHMARK_MAIN();
