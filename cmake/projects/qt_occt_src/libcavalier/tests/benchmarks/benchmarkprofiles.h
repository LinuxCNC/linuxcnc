#include "polyline.hpp"
#include <benchmark/benchmark.h>
struct TestProfile {
  std::size_t offsetCount;
  double offsetDelta;
  cavc::Polyline<double> pline;
  TestProfile(std::size_t offsetCount, double offsetDelta, cavc::Polyline<double> pline)
      : offsetCount(offsetCount), offsetDelta(offsetDelta), pline(pline) {}
};

inline TestProfile square() {
  auto centerX = 5.0;
  auto centerY = 5.0;
  auto width = 40.0;
  auto height = 40.0;
  cavc::Polyline<double> pline;
  pline.isClosed() = true;
  pline.addVertex(centerX - width / 2, centerY - height / 2, 0);
  pline.addVertex(centerX + width / 2, centerY - height / 2, 0);
  pline.addVertex(centerX + width / 2, centerY + height / 2, 0);
  pline.addVertex(centerX - width / 2, centerY + height / 2, 0);

  return TestProfile(30, 1, std::move(pline));
}

inline TestProfile diamond() {
  auto centerX = 5.0;
  auto centerY = 5.0;
  auto width = 40.0;
  auto height = 40.0;
  cavc::Polyline<double> pline;
  pline.isClosed() = true;
  pline.addVertex(centerX - width / 2, centerY, 0);
  pline.addVertex(centerX, centerY - height / 2, 0);
  pline.addVertex(centerX + width / 2, centerY, 0);
  pline.addVertex(centerX, centerY + height / 2, 0);

  return TestProfile(30, 1, std::move(pline));
}

inline TestProfile circle(double arcsToLinesError = 0.0) {
  auto centerX = 5.0;
  auto centerY = 5.0;
  auto radius = 40.0;
  cavc::Polyline<double> pline;
  pline.isClosed() = true;
  pline.addVertex(centerX - radius, centerY, 1.0);
  pline.addVertex(centerX + radius, centerY, 1.0);

  if (arcsToLinesError != 0.0) {
    pline = cavc::convertArcsToLines(pline, arcsToLinesError);
  }

  return TestProfile(30, 1, std::move(pline));
}

inline TestProfile roundedRectangle(double arcsToLinesError = 0.0) {
  auto centerX = 5.0;
  auto centerY = 5.0;
  auto totalWidth = 40.0;
  auto totalHeight = 20.0;
  auto cornerRadius = 5.0;
  auto width = totalWidth - 2 * cornerRadius;
  auto height = totalHeight - 2 * cornerRadius;
  // 90 deg corner radii
  auto bulge = std::tan(cavc::utils::pi<double>() / 8.0);

  cavc::Polyline<double> pline;
  pline.isClosed() = true;
  // start YMin edge
  pline.addVertex(centerX - width / 2, centerY - totalHeight / 2, 0);
  // start XMaxYMin radius
  pline.addVertex(centerX + width / 2, centerY - totalHeight / 2, bulge);
  // start XMax edge
  pline.addVertex(centerX + totalWidth / 2, centerY - height / 2, 0);
  // start XMaxYMax radius
  pline.addVertex(centerX + totalWidth / 2, centerY + height / 2, bulge);
  // start YMax edge
  pline.addVertex(centerX + width / 2, centerY + totalHeight / 2, 0);
  // start XMinYMax radius
  pline.addVertex(centerX - width / 2, centerY + totalHeight / 2, bulge);
  // start XMin edge
  pline.addVertex(centerX - totalWidth / 2, centerY + height / 2, 0);
  // start XMinYMin radius
  pline.addVertex(centerX - totalWidth / 2, centerY - height / 2, bulge);

  if (arcsToLinesError != 0.0) {
    pline = cavc::convertArcsToLines(pline, arcsToLinesError);
  }

  return TestProfile(30, 0.5, std::move(pline));
}

inline TestProfile profile1(double arcsToLinesError = 0.0) {
  cavc::Polyline<double> pline;
  pline.isClosed() = true;

  pline.addVertex(0, 0, 0.0);
  pline.addVertex(2, 0, 1.0);
  pline.addVertex(10, 0, -0.5);
  pline.addVertex(10, 10, 0.5);
  pline.addVertex(14, 20, -0.5);
  pline.addVertex(0, 20, 0);

  if (arcsToLinesError != 0.0) {
    pline = cavc::convertArcsToLines(pline, arcsToLinesError);
  }

  return TestProfile(40, 0.1, std::move(pline));
}

inline TestProfile profile2(double arcsToLinesError = 0.0) {
  cavc::Polyline<double> pline;
  pline.addVertex(0, 25, 1);
  pline.addVertex(0, 0, 0);
  pline.addVertex(2, 0, 1);
  pline.addVertex(10, 0, -0.5);
  pline.addVertex(8, 9, 0.374794619217547);
  pline.addVertex(21, 0, 0);
  pline.addVertex(23, 0, 1);
  pline.addVertex(32, 0, -0.5);
  pline.addVertex(28, 0, 0.5);
  pline.addVertex(39, 21, 0);
  pline.addVertex(28, 12, 0);
  pline.isClosed() = true;

  if (arcsToLinesError != 0.0) {
    pline = cavc::convertArcsToLines(pline, arcsToLinesError);
  }

  return TestProfile(40, 0.1, std::move(pline));
}

inline TestProfile pathologicalProfile1(std::size_t segmentCount, double arcsToLinesError = 0.0) {
  auto radius = 40;
  auto centerX = 0;
  auto centerY = 0;
  cavc::Polyline<double> pline;
  pline.isClosed() = true;

  for (std::size_t i = 0; i < segmentCount; ++i) {
    double angle = static_cast<double>(i) * cavc::utils::tau<double>() / segmentCount;
    pline.addVertex(radius * std::cos(angle) + centerX, radius * std::sin(angle) + centerY,
                    i % 2 == 0 ? 1 : -1);
  }

  if (arcsToLinesError != 0.0) {
    pline = cavc::convertArcsToLines(pline, arcsToLinesError);
  }

  return TestProfile(30, 1, std::move(pline));
}

struct NoSetup {
  NoSetup(TestProfile const &) {}
};

#define CAVC_BENCH_BODY(setupFunc, func)                                                           \
  auto setup = setupFunc(profile);                                                                 \
  state.counters["vertexCount"] = static_cast<double>(profile.pline.size());                       \
  for (auto _ : state) {                                                                           \
    (void)_;                                                                                       \
    func(setup, profile);                                                                          \
  }

#define CAVC_CREATE_SQUARE_BM(name, setupFunc, func, unit)                                         \
  static void BM_##name##Square(benchmark::State &state) {                                         \
    auto profile = square();                                                                       \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Square)->Unit(unit);

#define CAVC_CREATE_DIAMOND_BM(name, setupFunc, func, unit)                                        \
  static void BM_##name##Diamond(benchmark::State &state) {                                        \
    auto profile = diamond();                                                                      \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Diamond)->Unit(unit);

#define CAVC_CREATE_CIRCLE_BM(name, setupFunc, func, unit)                                         \
  static void BM_##name##Circle(benchmark::State &state) {                                         \
    auto profile = circle(0.0);                                                                    \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Circle)->Unit(unit);

#define CAVC_CREATE_ROUNDEDRECT_BM(name, setupFunc, func, unit)                                    \
  static void BM_##name##RoundedRect(benchmark::State &state) {                                    \
    auto profile = roundedRectangle(0.0);                                                          \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##RoundedRect)->Unit(unit);

#define CAVC_CREATE_PROFILE1_BM(name, setupFunc, func, unit)                                       \
  static void BM_##name##Profile1(benchmark::State &state) {                                       \
    auto profile = profile1(0.0);                                                                  \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Profile1)->Unit(unit);

#define CAVC_CREATE_PROFILE2_BM(name, setupFunc, func, unit)                                       \
  static void BM_##name##Profile2(benchmark::State &state) {                                       \
    auto profile = profile2(0.0);                                                                  \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Profile2)->Unit(unit);

#define CAVC_CREATE_PATHOLOGICAL1_BM(name, setupFunc, func, unit)                                  \
  static void BM_##name##Pathological1(benchmark::State &state) {                                  \
    auto profile = pathologicalProfile1(static_cast<std::size_t>(state.range(0)), 0.0);            \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Pathological1)->Unit(unit)->Arg(10)->Arg(25)->Arg(50)->Arg(100);

#define CAVC_CREATE_CIRCLE_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)               \
  static void BM_##name##CircleNoArcs(benchmark::State &state) {                                   \
    auto profile = circle(arcsToLinesError);                                                       \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##CircleNoArcs)->Unit(unit);

#define CAVC_CREATE_ROUNDEDRECT_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)          \
  static void BM_##name##RoundedRectNoArcs(benchmark::State &state) {                              \
    auto profile = roundedRectangle(arcsToLinesError);                                             \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##RoundedRectNoArcs)->Unit(unit);

#define CAVC_CREATE_PROFILE1_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)             \
  static void BM_##name##Profile1NoArcs(benchmark::State &state) {                                 \
    auto profile = profile1(arcsToLinesError);                                                     \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Profile1NoArcs)->Unit(unit);

#define CAVC_CREATE_PROFILE2_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)             \
  static void BM_##name##Profile2NoArcs(benchmark::State &state) {                                 \
    auto profile = profile2(arcsToLinesError);                                                     \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Profile2NoArcs)->Unit(unit);

#define CAVC_CREATE_PATHOLOGICAL1_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)        \
  static void BM_##name##Pathological1NoArcs(benchmark::State &state) {                            \
    auto profile =                                                                                 \
        pathologicalProfile1(static_cast<std::size_t>(state.range(0)), arcsToLinesError);          \
    CAVC_BENCH_BODY(setupFunc, func)                                                               \
  }                                                                                                \
  BENCHMARK(BM_##name##Pathological1NoArcs)->Unit(unit)->Arg(10)->Arg(25)->Arg(50)->Arg(100);

#define CAVC_CREATE_BENCHMARKS(name, setupFunc, func, unit)                                        \
  CAVC_CREATE_SQUARE_BM(name, setupFunc, func, unit)                                               \
  CAVC_CREATE_DIAMOND_BM(name, setupFunc, func, unit)                                              \
  CAVC_CREATE_CIRCLE_BM(name, setupFunc, func, unit)                                               \
  CAVC_CREATE_ROUNDEDRECT_BM(name, setupFunc, func, unit)                                          \
  CAVC_CREATE_PROFILE1_BM(name, setupFunc, func, unit)                                             \
  CAVC_CREATE_PROFILE2_BM(name, setupFunc, func, unit)                                             \
  CAVC_CREATE_PATHOLOGICAL1_BM(name, setupFunc, func, unit)

#define CAVC_CREATE_NO_ARCS_BENCHMARKS(name, setupFunc, func, arcsToLinesError, unit)              \
  CAVC_CREATE_CIRCLE_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)                     \
  CAVC_CREATE_ROUNDEDRECT_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)                \
  CAVC_CREATE_PROFILE1_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)                   \
  CAVC_CREATE_PROFILE2_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)                   \
  CAVC_CREATE_PATHOLOGICAL1_NO_ARCS_BM(name, setupFunc, func, arcsToLinesError, unit)
