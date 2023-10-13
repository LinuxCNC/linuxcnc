#include "polylineoffsetislands.hpp"

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // outer surrounding counter clockwise loop that surrounds the islands
  // (not required, it is valid to pass in just clockwise loops to be offset)
  cavc::Polyline<double> outerCCWLoop;
  outerCCWLoop.addVertex(-10, 1, 1);
  outerCCWLoop.addVertex(10, 1, 1);
  outerCCWLoop.isClosed() = true;

  cavc::Polyline<double> island1;
  island1.addVertex(-7, -5, 0);
  island1.addVertex(-4, -5, 0);
  island1.addVertex(-4, 5, 0);
  island1.addVertex(-7, 5, 0);
  island1.isClosed() = true;
  // NOTE: invert direction of island1 to make it go clockwise, all islands must be clockwise
  cavc::invertDirection(island1);

  cavc::Polyline<double> island2;
  island2.addVertex(5, -5, -1);
  island2.addVertex(3, 5, 0);
  island2.isClosed() = true;
  // NOTE: not inverting direction of island2 since we defined the vertexes to create a loop in a
  // clockwise direction

  cavc::OffsetLoopSet<double> loopSet;
  // add the surrounding counter clockwise loop
  // constructed with {parent index, closed polyline, spatial index}
  // this structure is also returned and can be fed back into the offset algorithm (the spatial
  // indexes are created by the algorithm and used for the next iteration)
  // NOTE: parent index can always be 0 (it is just used internally, API likely to be improved in
  // the future...), spatial index must always be created with the associated polyline
  loopSet.ccwLoops.push_back({0, outerCCWLoop, cavc::createApproxSpatialIndex(outerCCWLoop)});

  // add the clockwise loop islands
  loopSet.cwLoops.push_back({0, island1, cavc::createApproxSpatialIndex(island1)});
  loopSet.cwLoops.push_back({0, island2, cavc::createApproxSpatialIndex(island2)});

  // NOTE: this algorithm requires all polylines to be closed and non-self intersecting, and not
  // intersect any of the other input polylines
  cavc::ParallelOffsetIslands<double> alg;
  // NOTE: offset delta is always taken as an absolute magnitude
  // (negative values are the same as positive, to change offset direction you can change the
  // orientation of the loops e.g. from clockwise to counter clockwise)
  const double offsetDelta = 1.0;
  // compute first offset (offset by 1.0)
  cavc::OffsetLoopSet<double> offsetResult = alg.compute(loopSet, offsetDelta);
  // compute offset of first offset result (offset by another 1.0)
  cavc::OffsetLoopSet<double> offsetResult2 = alg.compute(offsetResult, offsetDelta);
}
