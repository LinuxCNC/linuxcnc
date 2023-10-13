#include "clipper/clipper.hpp"
#include "benchmarkprofiles.h"
#include "cavc/polyline.hpp"
#include <benchmark/benchmark.h>

const double clipperScaleFactor = 1e8;
const double unscaledArcError = 0.01;
const double clipperRoundPrecision = clipperScaleFactor * unscaledArcError;

static ClipperLib::Path polylineToClipperPath(const cavc::Polyline<double> &pline,
                                              double unscaledArcError) {
  auto noArcsPline = cavc::convertArcsToLines(pline, unscaledArcError);

  ClipperLib::Path clipperPath;
  clipperPath.reserve(noArcsPline.size());
  for (const auto &v : noArcsPline.vertexes()) {
    ClipperLib::cInt xInt = static_cast<ClipperLib::cInt>(v.x() * clipperScaleFactor);
    ClipperLib::cInt yInt = static_cast<ClipperLib::cInt>(v.y() * clipperScaleFactor);
    clipperPath.push_back(ClipperLib::IntPoint(xInt, yInt));
  }

  return clipperPath;
}

static void clipperOffset(const ClipperLib::Path &path, double delta, double roundPrecision,
                          ClipperLib::Paths &results) {
  ClipperLib::ClipperOffset clipperOffset(2, roundPrecision);
  clipperOffset.AddPath(path, ClipperLib::JoinType::jtRound, ClipperLib::EndType::etClosedPolygon);
  clipperOffset.Execute(results, delta);
}

struct ClipperSetup {
  ClipperLib::Path clipperPath;
  ClipperSetup(TestProfile const &profile)
      : clipperPath(polylineToClipperPath(profile.pline, unscaledArcError)) {}
};

static void offset(ClipperSetup const &setup, TestProfile const &profile) {
  // negate delta to match orientation that cavc uses
  double offsetDelta = -clipperScaleFactor * profile.offsetDelta;

  for (std::size_t i = 1; i <= profile.offsetCount; ++i) {
    double offset = i * offsetDelta;
    {
      ClipperLib::Paths results;
      clipperOffset(setup.clipperPath, offset, clipperRoundPrecision, results);
    }
    {
      ClipperLib::Paths results;
      clipperOffset(setup.clipperPath, -offset, clipperRoundPrecision, results);
    }
  }
}

CAVC_CREATE_BENCHMARKS(clipperOffset, ClipperSetup, offset, benchmark::kMillisecond)

BENCHMARK_MAIN();
