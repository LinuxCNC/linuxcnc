#ifndef CAVC_POLYLINECOMBINE_HPP
#define CAVC_POLYLINECOMBINE_HPP
#include "polyline.hpp"
#include "polylineintersects.hpp"
#include <unordered_map>
#include <vector>

// This header has functions for combining closed polylines (performing boolean operations)

namespace cavc {
namespace internal {
template <typename Real> struct ProcessForCombineResult {
  std::vector<Polyline<Real>> coincidentSlices;
  std::vector<PlineIntersect<Real>> nonCoincidentIntersects;
  std::vector<PlineIntersect<Real>> coincidentSliceStartPoints;
  std::vector<PlineIntersect<Real>> coincidentSliceEndPoints;
  std::vector<bool> coincidentIsOpposingDirection;
  bool pline1IsCW = false;
  bool pline2IsCW = false;
  bool completelyCoincident() const {
    return coincidentSliceStartPoints.size() == 1 && coincidentSliceEndPoints.size() == 1 &&
           fuzzyEqual(coincidentSliceStartPoints[0].pos, coincidentSliceEndPoints[0].pos,
                      utils::realPrecision<Real>());
  }

  bool plineOpposingDirections() const { return pline1IsCW != pline2IsCW; }

  bool anyIntersects() const {
    return coincidentSlices.size() != 0 || nonCoincidentIntersects.size() != 0;
  }
};

template <typename Real, std::size_t N>
ProcessForCombineResult<Real>
processForCombine(Polyline<Real> const &pline1, Polyline<Real> const &pline2,
                  StaticSpatialIndex<Real, N> const &pline1SpatialIndex) {

  CAVC_ASSERT(pline1.isClosed() && pline2.isClosed(), "combining only works with closed polylines");

  PlineIntersectsResult<Real> intrs;
  findIntersects(pline1, pline2, pline1SpatialIndex, intrs);

  ProcessForCombineResult<Real> result;
  result.pline1IsCW = getArea(pline1) < 0.0;
  result.pline2IsCW = getArea(pline2) < 0.0;
  result.nonCoincidentIntersects.swap(intrs.intersects);

  if (intrs.coincidentIntersects.size() == 0) {
    return result;
  }

  auto coincidentSliceResults =
      sortAndjoinCoincidentSlices(intrs.coincidentIntersects, pline1, pline2);
  result.coincidentSlices.swap(coincidentSliceResults.coincidentSlices);
  result.coincidentSliceStartPoints.swap(coincidentSliceResults.sliceStartPoints);
  result.coincidentSliceEndPoints.swap(coincidentSliceResults.sliceEndPoints);
  result.coincidentIsOpposingDirection.swap(coincidentSliceResults.coincidentIsOpposingDirection);

  return result;
}

template <typename Real> struct SlicePoint {
  Vector2<Real> pos;
  // indicates slice point is the start of a coincident slice
  bool startOfCoincidentSlice;
  SlicePoint(Vector2<Real> const &pos, bool startOfCoincidentSlice)
      : pos(pos), startOfCoincidentSlice(startOfCoincidentSlice) {}
};

/// Slice the given pline at all of its intersects for combining. If useSecondIndex is true then the
/// second index of the PlineIntersect type is used to correspond with pline, otherwise the first
/// index is used. pointOnSlicePred is called on at least one point from each slice, if it returns
/// true then the slice is kept, otherwise it is discarded. result is populated with open polylines
/// that represent all the slices.
template <typename Real, typename PointOnSlicePred>
void sliceAtIntersects(Polyline<Real> const &pline,
                       ProcessForCombineResult<Real> const &combineInfo, bool useSecondIndex,
                       PointOnSlicePred &&pointOnSlicePred, std::vector<Polyline<Real>> &result) {

  std::unordered_map<std::size_t, std::vector<SlicePoint<Real>>> intersectsLookup;
  intersectsLookup.reserve(combineInfo.nonCoincidentIntersects.size() +
                           combineInfo.coincidentSliceStartPoints.size() +
                           combineInfo.coincidentSliceEndPoints.size());

  if (useSecondIndex) {
    // use sIndex2 for lookup
    for (PlineIntersect<Real> const &intr : combineInfo.nonCoincidentIntersects) {
      intersectsLookup[intr.sIndex2].push_back(SlicePoint<Real>(intr.pos, false));
    }

    for (std::size_t i = 0; i < combineInfo.coincidentSliceStartPoints.size(); ++i) {
      auto const &sp = combineInfo.coincidentSliceStartPoints[i];
      auto const &ep = combineInfo.coincidentSliceEndPoints[i];
      if (combineInfo.coincidentIsOpposingDirection[i]) {
        intersectsLookup[sp.sIndex2].push_back(SlicePoint<Real>(sp.pos, false));
        intersectsLookup[ep.sIndex2].push_back(SlicePoint<Real>(ep.pos, true));
      } else {
        intersectsLookup[sp.sIndex2].push_back(SlicePoint<Real>(sp.pos, true));
        intersectsLookup[ep.sIndex2].push_back(SlicePoint<Real>(ep.pos, false));
      }
    }
  } else {
    // use sIndex1 for lookup
    for (PlineIntersect<Real> const &intr : combineInfo.nonCoincidentIntersects) {
      intersectsLookup[intr.sIndex1].push_back(SlicePoint<Real>(intr.pos, false));
    }
    for (PlineIntersect<Real> const &intr : combineInfo.coincidentSliceStartPoints) {
      intersectsLookup[intr.sIndex1].push_back(SlicePoint<Real>(intr.pos, true));
    }

    for (PlineIntersect<Real> const &intr : combineInfo.coincidentSliceEndPoints) {
      intersectsLookup[intr.sIndex1].push_back(SlicePoint<Real>(intr.pos, false));
    }
  }

  // sort intersects by distance from start vertex
  for (auto &kvp : intersectsLookup) {
    Vector2<Real> startPos = pline[kvp.first].pos();
    auto cmp = [&](SlicePoint<Real> const &i1, SlicePoint<Real> const &i2) {
      return distSquared(i1.pos, startPos) < distSquared(i2.pos, startPos);
    };
    std::sort(kvp.second.begin(), kvp.second.end(), cmp);
  }

  for (auto const &kvp : intersectsLookup) {
    // start index for the slice we're about to build
    std::size_t sIndex = kvp.first;
    // intersect list for this start index
    std::vector<SlicePoint<Real>> const &intrsList = kvp.second;

    const auto &firstSegStartVertex = pline[sIndex];
    std::size_t nextIndex = utils::nextWrappingIndex(sIndex, pline);
    const auto &firstSegEndVertex = pline[nextIndex];

    if (intrsList.size() != 1) {
      // build all the segments between the N intersects in siList (N > 1), skipping the first
      // segment (to be processed at the end)
      SplitResult<Real> firstSplit =
          splitAtPoint(firstSegStartVertex, firstSegEndVertex, intrsList[0].pos);
      auto prevVertex = firstSplit.splitVertex;
      for (std::size_t i = 1; i < intrsList.size(); ++i) {
        SplitResult<Real> split = splitAtPoint(prevVertex, firstSegEndVertex, intrsList[i].pos);
        // update prevVertex for next loop iteration
        prevVertex = split.splitVertex;

        if (intrsList[i - 1].startOfCoincidentSlice) {
          // skip coincident slices
          continue;
        }

        if (fuzzyEqual(split.updatedStart.pos(), split.splitVertex.pos(),
                       utils::realPrecision<Real>())) {
          continue;
        }

        auto sMidpoint = segMidpoint(split.updatedStart, split.splitVertex);
        if (!pointOnSlicePred(sMidpoint)) {
          // skip slice
          continue;
        }

        result.emplace_back();
        result.back().addVertex(split.updatedStart);
        result.back().addVertex(split.splitVertex);
      }
    }

    if (intrsList.back().startOfCoincidentSlice) {
      // skip coincident slices
      continue;
    }
    // build the segment between the last intersect in instrsList and the next intersect found
    SplitResult<Real> split =
        splitAtPoint(firstSegStartVertex, firstSegEndVertex, intrsList.back().pos);

    Polyline<Real> currSlice;
    currSlice.addVertex(split.splitVertex);

    std::size_t index = nextIndex;
    std::size_t loopCount = 0;
    const std::size_t maxLoopCount = pline.size();
    while (true) {
      if (loopCount++ > maxLoopCount) {
        CAVC_ASSERT(false, "Bug detected, should never loop this many times!");
        // break to avoid infinite loop
        break;
      }
      // add vertex
      internal::addOrReplaceIfSamePos(currSlice, pline[index]);

      // check if segment that starts at vertex we just added has an intersect
      auto nextIntr = intersectsLookup.find(index);
      if (nextIntr != intersectsLookup.end()) {
        // there is an intersect, slice is done
        Vector2<Real> const &intersectPos = nextIntr->second[0].pos;

        // trim last added vertex and add final intersect position
        PlineVertex<Real> endVertex = PlineVertex<Real>(intersectPos, Real(0));
        std::size_t nextIndex = utils::nextWrappingIndex(index, pline);
        SplitResult<Real> split =
            splitAtPoint(currSlice.lastVertex(), pline[nextIndex], intersectPos);
        currSlice.lastVertex() = split.updatedStart;
        internal::addOrReplaceIfSamePos(currSlice, endVertex);
        break;
      }
      // else there is not an intersect, increment index and continue
      index = utils::nextWrappingIndex(index, pline);
    }

    if (currSlice.size() > 1) {
      auto sMidpoint = segMidpoint(currSlice[currSlice.size() - 2], currSlice.lastVertex());
      if (pointOnSlicePred(sMidpoint)) {
        result.push_back(std::move(currSlice));
      }
    }
  }
}

/// Type to hold all the slices collected after slicing at intersects, slicesRemaining holds the
/// following: non-coincident slices from plineA, followed by non-coincident slices from plineB,
/// followed by coincident slices from plineA, followed by coincident slices from plineB
/// other fields hold the index markers
template <typename Real> struct CollectedSlices {
  std::vector<Polyline<Real>> slicesRemaining;
  std::size_t startOfPlineBSlicesIdx;
  std::size_t startOfPlineACoincidentSlicesIdx;
  std::size_t startOfPlineBCoincidentSlicesIdx;
};

template <typename Real, typename PlineAPointOnSlicePred, typename PlineBPointOnSlicePred>
CollectedSlices<Real> collectSlices(Polyline<Real> const &plineA, Polyline<Real> const &plineB,
                                    ProcessForCombineResult<Real> const &combineInfo,
                                    PlineAPointOnSlicePred &&plineAPointOnSlicePred,
                                    PlineBPointOnSlicePred &&plineBPointOnSlicePred,
                                    bool setOpposingOrientation) {
  CollectedSlices<Real> result;
  auto &slicesRemaining = result.slicesRemaining;

  // slice plineA
  sliceAtIntersects(plineA, combineInfo, false, plineAPointOnSlicePred, slicesRemaining);

  // slice plineB
  result.startOfPlineBCoincidentSlicesIdx = slicesRemaining.size();
  sliceAtIntersects(plineB, combineInfo, true, plineBPointOnSlicePred, slicesRemaining);

  // add plineA coincident slices
  result.startOfPlineACoincidentSlicesIdx = slicesRemaining.size();
  slicesRemaining.insert(slicesRemaining.end(), combineInfo.coincidentSlices.begin(),
                         combineInfo.coincidentSlices.end());

  // add plineB coincident slices
  std::size_t startOfPlineBCoincidentSlices = slicesRemaining.size();
  slicesRemaining.insert(slicesRemaining.end(), combineInfo.coincidentSlices.begin(),
                         combineInfo.coincidentSlices.end());

  // invert direction of plineB coincident slices to match original orientation
  std::size_t coincidentSliceIdx = 0;
  for (std::size_t i = startOfPlineBCoincidentSlices; i < slicesRemaining.size(); ++i) {
    if (combineInfo.coincidentIsOpposingDirection[coincidentSliceIdx]) {
      invertDirection(slicesRemaining[i]);
    }
    ++coincidentSliceIdx;
  }

  if (setOpposingOrientation != combineInfo.plineOpposingDirections()) {
    // invert plineB slice directions to match request of setOpposingOrientation
    for (std::size_t i = result.startOfPlineBSlicesIdx; i < result.startOfPlineACoincidentSlicesIdx;
         ++i) {
      invertDirection(slicesRemaining[i]);
    }

    for (std::size_t i = result.startOfPlineBCoincidentSlicesIdx; i < slicesRemaining.size(); ++i) {
      invertDirection(slicesRemaining[i]);
    }
  }

  return result;
}

struct StitchFirstAvailable {
  std::size_t operator()(std::size_t currSliceIndex, std::vector<std::size_t> const &available) {
    (void)currSliceIndex;
    return available[0];
  }
};

/// Stitches open polyline slices together into closed polylines. The open polylines must be
/// ordered/agree on direction (every start point connects with an end point). sitchSelector may be
/// used to determine priority of stitching in the case multiple possibilities exist.
template <typename Real, typename StitchSelector = StitchFirstAvailable>
std::vector<Polyline<Real>>
stitchOrderedSlicesIntoClosedPolylines(std::vector<Polyline<Real>> const &slices,
                                       StitchSelector stitchSelector = StitchFirstAvailable(),
                                       Real joinThreshold = utils::sliceJoinThreshold<Real>()) {
  std::vector<Polyline<Real>> result;
  if (slices.size() == 0) {
    return result;
  }

  // load all the slice start points into spatial index
  StaticSpatialIndex<Real> spatialIndex(slices.size());
  auto addEndPoint = [&](Vector2<Real> const &pt) {
    spatialIndex.add(pt.x() - joinThreshold, pt.y() - joinThreshold, pt.x() + joinThreshold,
                     pt.y() + joinThreshold);
  };

  for (const auto &slice : slices) {
    addEndPoint(slice[0].pos());
  }
  spatialIndex.finish();

  std::vector<bool> visitedSliceIndexes(slices.size(), false);

  std::vector<std::size_t> queryResults;
  std::vector<std::size_t> queryStack;
  queryStack.reserve(8);

  auto closePline = [&](Polyline<Real> &pline) {
    if (pline.size() < 3) {
      // skip slice in the case of just two vertexes ontop of each other
      return;
    }
    pline.vertexes().pop_back();
    pline.isClosed() = true;
    result.emplace_back();
    using namespace std;
    swap(pline, result.back());
  };

  // loop through all slice indexes
  for (std::size_t i = 0; i < slices.size(); ++i) {
    if (visitedSliceIndexes[i]) {
      continue;
    }
    visitedSliceIndexes[i] = true;

    // create new polyline
    Polyline<Real> currPline;
    currPline.vertexes().insert(currPline.vertexes().end(), slices[i].vertexes().begin(),
                                slices[i].vertexes().end());

    const std::size_t beginningSliceIndex = i;
    std::size_t currSliceIndex = i;
    std::size_t loopCount = 0;
    const std::size_t maxLoopCount = slices.size();
    while (true) {
      if (loopCount++ > maxLoopCount) {
        CAVC_ASSERT(false, "Bug detected, should never loop this many times!");
        // break to avoid infinite loop
        break;
      }
      const auto &currEndPoint = currPline.lastVertex().pos();
      queryResults.clear();
      spatialIndex.query(currEndPoint.x() - joinThreshold, currEndPoint.y() - joinThreshold,
                         currEndPoint.x() + joinThreshold, currEndPoint.y() + joinThreshold,
                         queryResults, queryStack);

      // skip if only index found corresponds to current index
      queryResults.erase(std::remove_if(queryResults.begin(), queryResults.end(),
                                        [&](std::size_t index) {
                                          return index != beginningSliceIndex &&
                                                 visitedSliceIndexes[index];
                                        }),
                         queryResults.end());

      if (queryResults.size() == 0) {
        // may arrive here due to thresholding around coincident segments, just discard it
        break;
      }

      std::size_t connectedSliceIndex = stitchSelector(currSliceIndex, queryResults);
      if (connectedSliceIndex == std::numeric_limits<std::size_t>::max()) {
        // discard current polyline
        break;
      }
      if (connectedSliceIndex == beginningSliceIndex) {
        closePline(currPline);
        break;
      }
      const auto &connectedSlice = slices[connectedSliceIndex];
      currPline.vertexes().pop_back();
      currPline.vertexes().insert(currPline.vertexes().end(), connectedSlice.vertexes().begin(),
                                  connectedSlice.vertexes().end());
      visitedSliceIndexes[connectedSliceIndex] = true;

      // else continue stitching slices to current polyline, using last stitched index to find next
      currSliceIndex = connectedSliceIndex;
    }
  }

  return result;
}
} // namespace internal

/// Combine mode to apply to closed polylines, corresponds to the various boolean operations that
/// are possible on polygons.
/// Union(A, B) = A OR B.
/// Exclude(A, B) = A NOT B.
/// Intersect(A, B) = A AND B.
/// XOR(A, B) = A XOR B.
enum class PlineCombineMode { Union, Exclude, Intersect, XOR };

/// Type to hold result of combining closed polylines. remaining holds the resulting closed
/// polylines after the operation. subtracted holds closed polylines that represent subtracted space
/// (these polylines are always fully enclosed by one the polylines in remaining).
template <typename Real> struct CombineResult {
  std::vector<Polyline<Real>> remaining;
  std::vector<Polyline<Real>> subtracted;
};

/// Combine two closed polylines applying a particular combine mode (boolean operation).
template <typename Real>
CombineResult<Real> combinePolylines(Polyline<Real> const &plineA, Polyline<Real> const &plineB,
                                     PlineCombineMode combineMode) {
  CAVC_ASSERT(plineA.isClosed() && plineB.isClosed(), "combining only supports closed polylines");
  using namespace internal;

  auto plASpatialIndex = createApproxSpatialIndex(plineA);
  ProcessForCombineResult<Real> combineInfo = processForCombine(plineA, plineB, plASpatialIndex);

  CombineResult<Real> result;

  // helper function test if point is inside A
  auto pointInA = [&](Vector2<Real> const &pt) { return getWindingNumber(plineA, pt) != 0; };
  // helper function test if point is inside B
  auto pointInB = [&](Vector2<Real> const &pt) { return getWindingNumber(plineB, pt) != 0; };

  // helper functions (assuming no intersects between A and B)
  auto isAInsideB = [&] { return pointInB(plineA[0].pos()); };
  auto isBInsideA = [&] { return pointInA(plineB[0].pos()); };

  auto createUnionAndIntersectStitchSelector = [](std::size_t startOfCoincidentSlicesIdx) {
    return [=](std::size_t currSliceIndex, std::vector<std::size_t> const &available) {
      // attempt to select noncoincident slice
      auto idx = std::find_if(available.begin(), available.end(),
                              [&](auto i) { return i < startOfCoincidentSlicesIdx; });
      if (idx == available.end()) {
        // no noncoincident slices available
        if (currSliceIndex >= startOfCoincidentSlicesIdx) {
          // stitching coincident to coincident is never allowed (discarded)
          return std::numeric_limits<std::size_t>::max();
        }
        // use first available
        return available[0];
      }

      // use first noncoincident slice
      return *idx;
    };
  };

  auto createExcludeAndXORStitchSelector = [](std::size_t startOfPlineBSlicesIdx,
                                              std::size_t startOfPlineACoincidentSlicesIdx,
                                              std::size_t startOfPlineBCoincidentSlicesIdx) {
    return [=](std::size_t currSliceIndex, std::vector<std::size_t> const &available) {
      if (currSliceIndex >= startOfPlineACoincidentSlicesIdx) {
        // current slice is coincident
        if (currSliceIndex < startOfPlineBCoincidentSlicesIdx) {
          // current coincident slice is from A, attempt to stitch to slice from plineB
          auto idx = std::find_if(available.begin(), available.end(), [&](auto i) {
            return i >= startOfPlineBSlicesIdx && i < startOfPlineACoincidentSlicesIdx;
          });

          if (idx != available.end()) {
            // stitch to plineB slice
            return *idx;
          }

          // attempt to stitch to slice from plineA
          idx = std::find_if(available.begin(), available.end(),
                             [&](auto i) { return i < startOfPlineBSlicesIdx; });

          if (idx != available.end()) {
            // stitch to plineA slice
            return *idx;
          }

          // no noncoincident slices available and stitching coincident to coincident is never
          // allowed (discarded)
          return std::numeric_limits<std::size_t>::max();
        }
        // else current coincident slice is from B, attempt to stitch to slice from plineA
        auto idx = std::find_if(available.begin(), available.end(),
                                [&](auto i) { return i < startOfPlineBSlicesIdx; });

        if (idx != available.end()) {
          // stitch to plineA slice
          return *idx;
        }

        // attempt to stitch to slice from plineB
        idx = std::find_if(available.begin(), available.end(),
                           [&](auto i) { return i < startOfPlineACoincidentSlicesIdx; });

        if (idx != available.end()) {
          // stitch to plineB slice
          return *idx;
        }

        // no noncoincident slices available and stitching coincident to coincident is never
        // allowed (discarded)
        return std::numeric_limits<std::size_t>::max();
      } else if (currSliceIndex < startOfPlineBSlicesIdx) {
        // current slice is from plineA, attempt to stitch to slice from plineB
        auto idx = std::find_if(available.begin(), available.end(), [&](auto i) {
          return i >= startOfPlineBSlicesIdx && i < startOfPlineACoincidentSlicesIdx;
        });

        if (idx != available.end()) {
          return *idx;
        }

        return available[0];
      }
      // else current slice is from plineB, attempt to stitch to slice from plineA
      auto idx = std::find_if(available.begin(), available.end(),
                              [&](auto i) { return i < startOfPlineBSlicesIdx; });

      if (idx != available.end()) {
        return *idx;
      }

      return available[0];
    };
  };

  auto performUnion = [&] {
    if (combineInfo.completelyCoincident()) {
      result.remaining.push_back(plineA);
      return;
    }
    if (!combineInfo.anyIntersects()) {
      if (isAInsideB()) {
        result.remaining.push_back(plineB);
      } else if (isBInsideA()) {
        result.remaining.push_back(plineA);
      } else {
        result.remaining.push_back(plineA);
        result.remaining.push_back(plineB);
      }
    } else {
      // keep all slices on A that are not in B and all slices on B that are not in A
      auto collectedSlices = collectSlices(
          plineA, plineB, combineInfo, [&](auto pt) { return !pointInB(pt); },
          [&](auto pt) { return !pointInA(pt); }, false);

      auto stitchSelector =
          createUnionAndIntersectStitchSelector(collectedSlices.startOfPlineACoincidentSlicesIdx);

      std::vector<Polyline<Real>> remaining =
          stitchOrderedSlicesIntoClosedPolylines(collectedSlices.slicesRemaining, stitchSelector);

      for (std::size_t i = 0; i < remaining.size(); ++i) {
        const bool isCW = getArea(remaining[i]) < Real(0);
        if (isCW != combineInfo.pline1IsCW) {
          // orientation flipped from original, therefore it is a subtracted island
          result.subtracted.push_back(std::move(remaining[i]));
        } else {
          // orientation stayed the same, just add to remaining
          result.remaining.push_back(std::move(remaining[i]));
        }
      }
    }
  };

  auto performExclude = [&] {
    if (combineInfo.completelyCoincident()) {
      // nothing left
      return;
    }
    if (!combineInfo.anyIntersects()) {
      if (isAInsideB()) {
        // no results (everything excluded)
      } else if (isBInsideA()) {
        // island created inside A
        result.remaining.push_back(plineA);
        result.subtracted.push_back(plineB);
      } else {
        // no overlap
        result.remaining.push_back(plineA);
      }
    } else {
      // keep all slices on A that are not in B and all slices on B that are in A
      auto collectedSlices = collectSlices(
          plineA, plineB, combineInfo, [&](auto pt) { return !pointInB(pt); }, pointInA, true);

      auto stitchSelector = createExcludeAndXORStitchSelector(
          collectedSlices.startOfPlineBSlicesIdx, collectedSlices.startOfPlineACoincidentSlicesIdx,
          collectedSlices.startOfPlineBCoincidentSlicesIdx);
      result.remaining =
          stitchOrderedSlicesIntoClosedPolylines(collectedSlices.slicesRemaining, stitchSelector);
    }
  };

  auto performIntersect = [&] {
    if (combineInfo.completelyCoincident()) {
      result.remaining.push_back(plineA);
      return;
    }
    if (!combineInfo.anyIntersects()) {
      if (isAInsideB()) {
        result.remaining.push_back(plineA);
      } else if (isBInsideA()) {
        result.remaining.push_back(plineB);
      } // else no overlap
    } else {
      // keep all slices on A that are in B and all slices on B that are in A
      auto collectedSlices = collectSlices(plineA, plineB, combineInfo, pointInB, pointInA, false);

      auto stitchSelector =
          createUnionAndIntersectStitchSelector(collectedSlices.startOfPlineACoincidentSlicesIdx);

      result.remaining =
          stitchOrderedSlicesIntoClosedPolylines(collectedSlices.slicesRemaining, stitchSelector);
    }
  };

  auto performXOR = [&] {
    if (combineInfo.completelyCoincident()) {
      return;
    }
    if (!combineInfo.anyIntersects()) {
      if (isAInsideB()) {
        result.remaining.push_back(plineB);
        result.subtracted.push_back(plineA);
      } else if (isBInsideA()) {
        result.remaining.push_back(plineA);
        result.subtracted.push_back(plineB);
      } else {
        result.remaining.push_back(plineA);
        result.remaining.push_back(plineB);
      }
    } else {

      // collect A excluding B results
      {
        // keep all slices on A that are not in B and all slices on B that are in A
        auto collectedSlices = collectSlices(
            plineA, plineB, combineInfo, [&](auto pt) { return !pointInB(pt); }, pointInA, true);

        auto stitchSelector =
            createExcludeAndXORStitchSelector(collectedSlices.startOfPlineBSlicesIdx,
                                              collectedSlices.startOfPlineACoincidentSlicesIdx,
                                              collectedSlices.startOfPlineBCoincidentSlicesIdx);
        result.remaining =
            stitchOrderedSlicesIntoClosedPolylines(collectedSlices.slicesRemaining, stitchSelector);
      }

      // collect B excluding A results
      {
        // keep all slices on A that are in B and all slices on B that are not in A
        auto collectedSlices = collectSlices(
            plineA, plineB, combineInfo, pointInB, [&](auto pt) { return !pointInA(pt); }, true);

        auto stitchSelector =
            createExcludeAndXORStitchSelector(collectedSlices.startOfPlineBSlicesIdx,
                                              collectedSlices.startOfPlineACoincidentSlicesIdx,
                                              collectedSlices.startOfPlineBCoincidentSlicesIdx);
        auto stitchedResults =
            stitchOrderedSlicesIntoClosedPolylines(collectedSlices.slicesRemaining, stitchSelector);
        for (auto &r : stitchedResults) {
          result.remaining.push_back(std::move(r));
        }
      }
    }
  };

  switch (combineMode) {
  case PlineCombineMode::Union:
    performUnion();
    break;
  case PlineCombineMode::Exclude:
    performExclude();
    break;
  case PlineCombineMode::Intersect:
    performIntersect();
    break;
  case PlineCombineMode::XOR:
    performXOR();
    break;
  }

  return result;
}
} // namespace cavc
#endif // CAVC_POLYLINECOMBINE_HPP
