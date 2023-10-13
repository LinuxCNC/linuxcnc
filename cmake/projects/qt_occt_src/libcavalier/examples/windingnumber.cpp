#include "polyline.hpp"

using namespace cavc;

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  // rectangle going counter clockwise
  Polyline<double> ccwRectangle;
  ccwRectangle.addVertex(-10, -10, 0);
  ccwRectangle.addVertex(10, -10, 0);
  ccwRectangle.addVertex(10, 10, 0);
  ccwRectangle.addVertex(-10, 10, 0);
  ccwRectangle.isClosed() = true;

  int wn = getWindingNumber(ccwRectangle, Vector2<double>(1, 1));
  assert(wn == 1);
  wn = getWindingNumber(ccwRectangle, Vector2<double>(12, 12));
  assert(wn == 0);

  // make rectangle go clockwise
  Polyline<double> cwRectangle = ccwRectangle;
  invertDirection(cwRectangle);
  wn = getWindingNumber(cwRectangle, Vector2<double>(1, 1));
  assert(wn == -1);

  // wrap around the point twice (self intersecting polyline)
  Polyline<double> ccwTwiceWrapping = ccwRectangle;
  ccwTwiceWrapping.addVertex(-12, -12, 1);
  ccwTwiceWrapping.addVertex(12, -12, 0);
  ccwTwiceWrapping.addVertex(12, 12, 0);
  ccwTwiceWrapping.addVertex(-12, 12, 0);
  wn = getWindingNumber(ccwTwiceWrapping, Vector2<double>(1, 1));
  assert(wn == 2);

  Polyline<double> cwTwiceWrapping = ccwTwiceWrapping;
  invertDirection(cwTwiceWrapping);
  wn = getWindingNumber(cwTwiceWrapping, Vector2<double>(1, 1));
  assert(wn == -2);

  // winding number function always returns 0 for open polylines whose start and end points do not
  // touch (even if geometrically it does wrap)
  ccwTwiceWrapping.isClosed() = false;
  wn = getWindingNumber(ccwTwiceWrapping, Vector2<double>(1, 1));
  assert(wn == 0);
}
