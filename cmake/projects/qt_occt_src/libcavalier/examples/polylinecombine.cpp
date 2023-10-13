#include "polylinecombine.hpp"

using namespace cavc;

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // closed polyline representing a circle
  Polyline<double> circle;
  circle.addVertex(0, 1, 1);
  circle.addVertex(10, 1, 1);
  circle.isClosed() = true;

  // closed polyline representing a rectangle (overlaps with the circle)
  Polyline<double> rectangle;
  rectangle.addVertex(3, -10, 0);
  rectangle.addVertex(6, -10, 0);
  rectangle.addVertex(6, 10, 0);
  rectangle.addVertex(3, 10, 0);
  rectangle.isClosed() = true;

  CombineResult<double> unionResult = combinePolylines(circle, rectangle, PlineCombineMode::Union);
  CombineResult<double> excludeResult =
      combinePolylines(circle, rectangle, PlineCombineMode::Exclude);
  CombineResult<double> intersectResult =
      combinePolylines(circle, rectangle, PlineCombineMode::Intersect);
  CombineResult<double> xorResult = combinePolylines(circle, rectangle, PlineCombineMode::XOR);
}
