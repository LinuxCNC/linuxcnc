#include "cavc/staticspatialindex.hpp"
#include "testhelpers.hpp"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <vector>

namespace t = testing;

std::vector<double> createTestData() {
  return std::vector<double>{
      8,  62, 11, 66, 57, 17, 57, 19, 76, 26, 79, 29, 36, 56, 38, 56, 92, 77, 96, 80, 87, 70, 90,
      74, 43, 41, 47, 43, 0,  58, 2,  62, 76, 86, 80, 89, 27, 13, 27, 15, 71, 63, 75, 67, 25, 2,
      27, 2,  87, 6,  88, 6,  22, 90, 23, 93, 22, 89, 22, 93, 57, 11, 61, 13, 61, 55, 63, 56, 17,
      85, 21, 87, 33, 43, 37, 43, 6,  1,  7,  3,  80, 87, 80, 87, 23, 50, 26, 52, 58, 89, 58, 89,
      12, 30, 15, 34, 32, 58, 36, 61, 41, 84, 44, 87, 44, 18, 44, 19, 13, 63, 15, 67, 52, 70, 54,
      74, 57, 59, 58, 59, 17, 90, 20, 92, 48, 53, 52, 56, 92, 68, 92, 72, 26, 52, 30, 52, 56, 23,
      57, 26, 88, 48, 88, 48, 66, 13, 67, 15, 7,  82, 8,  86, 46, 68, 50, 68, 37, 33, 38, 36, 6,
      15, 8,  18, 85, 36, 89, 38, 82, 45, 84, 48, 12, 2,  16, 3,  26, 15, 26, 16, 55, 23, 59, 26,
      76, 37, 79, 39, 86, 74, 90, 77, 16, 75, 18, 78, 44, 18, 45, 21, 52, 67, 54, 71, 59, 78, 62,
      78, 24, 5,  24, 8,  64, 80, 64, 83, 66, 55, 70, 55, 0,  17, 2,  19, 15, 71, 18, 74, 87, 57,
      87, 59, 6,  34, 7,  37, 34, 30, 37, 32, 51, 19, 53, 19, 72, 51, 73, 55, 29, 45, 30, 45, 94,
      94, 96, 95, 7,  22, 11, 24, 86, 45, 87, 48, 33, 62, 34, 65, 18, 10, 21, 14, 64, 66, 67, 67,
      64, 25, 65, 28, 27, 4,  31, 6,  84, 4,  85, 5,  48, 80, 50, 81, 1,  61, 3,  61, 71, 89, 74,
      92, 40, 42, 43, 43, 27, 64, 28, 66, 46, 26, 50, 26, 53, 83, 57, 87, 14, 75, 15, 79, 31, 45,
      34, 45, 89, 84, 92, 88, 84, 51, 85, 53, 67, 87, 67, 89, 39, 26, 43, 27, 47, 61, 47, 63, 23,
      49, 25, 53, 12, 3,  14, 5,  16, 50, 19, 53, 63, 80, 64, 84, 22, 63, 22, 64, 26, 66, 29, 66,
      2,  15, 3,  15, 74, 77, 77, 79, 64, 11, 68, 11, 38, 4,  39, 8,  83, 73, 87, 77, 85, 52, 89,
      56, 74, 60, 76, 63, 62, 66, 65, 67};
}

static std::vector<double> testData = createTestData();

cavc::StaticSpatialIndex<double, 16> createIndex() {
  cavc::StaticSpatialIndex<double, 16> index(testData.size() / 4);
  for (std::size_t i = 0; i < testData.size(); i += 4) {
    index.add(testData[i], testData[i + 1], testData[i + 2], testData[i + 3]);
  }
  index.finish();
  return index;
};

static cavc::StaticSpatialIndex<double, 16> createSmallIndex() {
  std::size_t numItems = 14;
  cavc::StaticSpatialIndex<double, 16> index(numItems);
  for (std::size_t i = 0; i < 4 * numItems; i += 4) {
    index.add(testData[i], testData[i + 1], testData[i + 2], testData[i + 3]);
  }
  index.finish();
  return index;
}

struct Box {
  double minX;
  double minY;
  double maxX;
  double maxY;

  std::string toInitList() const {
    std::stringstream ss;
    ss << "{" << minX << "," << minY << "," << maxX << "," << maxY << "}";
    return ss.str();
  }
};

std::ostream &operator<<(std::ostream &os, Box const &box) {
  os << "{" << box.minX << ", " << box.minY << ", " << box.maxX << ", " << box.maxY << "}";
  return os;
}

bool operator==(Box const &left, Box const &right) {
  return fuzzyEqual(left.minX, right.minX) && fuzzyEqual(left.minY, right.minY) &&
         fuzzyEqual(left.maxX, right.maxX) && fuzzyEqual(left.maxY, right.maxY);
}

TEST(StaticSpatialIndexTests, index) {
  auto index = createIndex();
  ASSERT_EQ(index.minX(), 0);
  ASSERT_EQ(index.minY(), 1);
  ASSERT_EQ(index.maxX(), 96);
  ASSERT_EQ(index.maxY(), 95);

  std::vector<std::size_t> levelBounds;
  std::size_t currLevel = std::numeric_limits<std::size_t>::max();
  std::vector<std::vector<Box>> levelBoxes;
  auto visitor = [&](std::size_t level, double minX, double minY, double maxX, double maxY) {
    if (currLevel != level) {
      currLevel = level;
      levelBounds.push_back(1);
      levelBoxes.emplace_back();
    } else {
      levelBounds.back() += 1;
    }

    levelBoxes.back().push_back({minX, minY, maxX, maxY});
    return true;
  };

  index.visitBoundingBoxes(visitor);

  ASSERT_THAT(levelBounds, t::SizeIs(3));

  std::vector<std::size_t> expectedLevelBounds = {1, 7, 100};
  ASSERT_THAT(levelBounds, t::ContainerEq(expectedLevelBounds));

  ASSERT_THAT(levelBoxes[0], t::SizeIs(1));
  Box expectedRootNodeBox{0, 1, 96, 95};
  ASSERT_EQ(levelBoxes[0][0], expectedRootNodeBox);

  std::vector<Box> expectedFirstTierBoxes = {{0, 1, 45, 24},   {6, 26, 50, 67},  {0, 58, 50, 93},
                                             {23, 50, 70, 89}, {59, 60, 96, 95}, {51, 13, 89, 59},
                                             {57, 4, 88, 13}};
  ASSERT_THAT(levelBoxes[1], t::Pointwise(t::Eq(), expectedFirstTierBoxes));

  // reverse the value boxes (they are visited in reverse order due to tree stack traversal)
  std::reverse(levelBoxes[2].begin(), levelBoxes[2].end());

  // create actual node size blocks of boxes to be compared with expected
  std::vector<std::vector<Box>> valueNodeBoxes;
  std::size_t nodeSize = 16;
  std::size_t valueNodeCount = (levelBoxes[2].size() - 1) / nodeSize + 1;
  valueNodeBoxes.reserve(valueNodeCount);

  for (std::size_t k = 0; k < valueNodeCount; ++k) {
    std::size_t start = k * nodeSize;
    std::size_t end = start + nodeSize;

    if (end > levelBoxes[2].size()) {
      end = levelBoxes[2].size();
    }

    valueNodeBoxes.emplace_back();
    valueNodeBoxes.back().reserve(end - start);
    for (std::size_t i = start; i < end; ++i) {
      valueNodeBoxes.back().push_back(levelBoxes[2][i]);
    }
  }

  // note on all these compares are unordered (value boxes within the same node are allowed to be
  // unordered and may be unordered for performance optimizations)
  std::vector<Box> expectedFirstValueNodeBoxes = {
      {6, 1, 7, 3},     {2, 15, 3, 15},   {0, 17, 2, 19},   {7, 22, 11, 24},
      {6, 15, 8, 18},   {18, 10, 21, 14}, {12, 3, 14, 5},   {12, 2, 16, 3},
      {24, 5, 24, 8},   {25, 2, 27, 2},   {27, 4, 31, 6},   {38, 4, 39, 8},
      {44, 18, 44, 19}, {44, 18, 45, 21}, {26, 15, 26, 16}, {27, 13, 27, 15}};
  ASSERT_THAT(valueNodeBoxes[0], t::UnorderedPointwise(t::Eq(), expectedFirstValueNodeBoxes));

  std::vector<Box> expectedSecondValueNodeBoxes = {
      {34, 30, 37, 32}, {39, 26, 43, 27}, {46, 26, 50, 26}, {37, 33, 38, 36},
      {43, 41, 47, 43}, {40, 42, 43, 43}, {31, 45, 34, 45}, {33, 43, 37, 43},
      {29, 45, 30, 45}, {12, 30, 15, 34}, {6, 34, 7, 37},   {16, 50, 19, 53},
      {23, 49, 25, 53}, {13, 63, 15, 67}, {22, 63, 22, 64}, {8, 62, 11, 66}};
  ASSERT_THAT(valueNodeBoxes[1], t::UnorderedPointwise(t::Eq(), expectedSecondValueNodeBoxes));

  std::vector<Box> expectedThirdValueNodeBoxes = {
      {1, 61, 3, 61},   {0, 58, 2, 62},   {7, 82, 8, 86},   {17, 90, 20, 92},
      {22, 90, 23, 93}, {22, 89, 22, 93}, {17, 85, 21, 87}, {16, 75, 18, 78},
      {14, 75, 15, 79}, {15, 71, 18, 74}, {41, 84, 44, 87}, {46, 68, 50, 68},
      {47, 61, 47, 63}, {26, 66, 29, 66}, {27, 64, 28, 66}, {33, 62, 34, 65}};
  ASSERT_THAT(valueNodeBoxes[2], t::UnorderedPointwise(t::Eq(), expectedThirdValueNodeBoxes));

  std::vector<Box> expectedFourthValueNodeBoxes = {
      {32, 58, 36, 61}, {26, 52, 30, 52}, {23, 50, 26, 52}, {36, 56, 38, 56},
      {48, 53, 52, 56}, {57, 59, 58, 59}, {66, 55, 70, 55}, {61, 55, 63, 56},
      {64, 66, 67, 67}, {62, 66, 65, 67}, {52, 67, 54, 71}, {52, 70, 54, 74},
      {48, 80, 50, 81}, {58, 89, 58, 89}, {53, 83, 57, 87}, {67, 87, 67, 89}};
  ASSERT_THAT(valueNodeBoxes[3], t::UnorderedPointwise(t::Eq(), expectedFourthValueNodeBoxes));

  std::vector<Box> expectedFifthValueNodeBoxes = {
      {64, 80, 64, 83}, {63, 80, 64, 84}, {59, 78, 62, 78}, {74, 77, 77, 79},
      {76, 86, 80, 89}, {71, 89, 74, 92}, {80, 87, 80, 87}, {94, 94, 96, 95},
      {89, 84, 92, 88}, {92, 77, 96, 80}, {86, 74, 90, 77}, {83, 73, 87, 77},
      {87, 70, 90, 74}, {92, 68, 92, 72}, {71, 63, 75, 67}, {74, 60, 76, 63}};
  ASSERT_THAT(valueNodeBoxes[4], t::UnorderedPointwise(t::Eq(), expectedFifthValueNodeBoxes));

  std::vector<Box> expectedSixthValueNodeBoxes = {
      {72, 51, 73, 55}, {84, 51, 85, 53}, {85, 52, 89, 56}, {87, 57, 87, 59},
      {88, 48, 88, 48}, {86, 45, 87, 48}, {85, 36, 89, 38}, {76, 26, 79, 29},
      {76, 37, 79, 39}, {82, 45, 84, 48}, {64, 25, 65, 28}, {66, 13, 67, 15},
      {55, 23, 59, 26}, {56, 23, 57, 26}, {51, 19, 53, 19}, {57, 17, 57, 19}};
  ASSERT_THAT(valueNodeBoxes[5], t::UnorderedPointwise(t::Eq(), expectedSixthValueNodeBoxes));

  std::vector<Box> expectedSeventhValueNodeBoxes = {
      {57, 11, 61, 13}, {64, 11, 68, 11}, {87, 6, 88, 6}, {84, 4, 85, 5}};
  ASSERT_THAT(valueNodeBoxes[6], t::UnorderedPointwise(t::Eq(), expectedSeventhValueNodeBoxes));
}

TEST(StaticSpatialIndexTests, skip_sorting_small_index) {
  auto index = createSmallIndex();

  ASSERT_EQ(index.minX(), 0);
  ASSERT_EQ(index.minY(), 2);
  ASSERT_EQ(index.maxX(), 96);
  ASSERT_EQ(index.maxY(), 93);

  std::vector<std::size_t> levelBounds;
  std::size_t currLevel = std::numeric_limits<std::size_t>::max();
  std::vector<std::vector<Box>> levelBoxes;
  auto visitor = [&](std::size_t level, double minX, double minY, double maxX, double maxY) {
    if (currLevel != level) {
      currLevel = level;
      levelBounds.push_back(1);
      levelBoxes.emplace_back();
    } else {
      levelBounds.back() += 1;
    }

    levelBoxes.back().push_back({minX, minY, maxX, maxY});
    return true;
  };

  index.visitBoundingBoxes(visitor);

  ASSERT_THAT(levelBounds, t::SizeIs(2));

  std::vector<std::size_t> expectedLevelBounds = {1, 14};
  ASSERT_THAT(levelBounds, t::ContainerEq(expectedLevelBounds));

  ASSERT_THAT(levelBoxes[0], t::SizeIs(1));
  Box expectedRootNodeBox{0, 2, 96, 93};
  ASSERT_EQ(levelBoxes[0][0], expectedRootNodeBox);

  std::vector<Box> expectedValueBoxes = {
      {8, 62, 11, 66},  {57, 17, 57, 19}, {76, 26, 79, 29}, {36, 56, 38, 56}, {92, 77, 96, 80},
      {87, 70, 90, 74}, {43, 41, 47, 43}, {0, 58, 2, 62},   {76, 86, 80, 89}, {27, 13, 27, 15},
      {71, 63, 75, 67}, {25, 2, 27, 2},   {87, 6, 88, 6},   {22, 90, 23, 93}};

  ASSERT_THAT(levelBoxes[1], t::ContainerEq(expectedValueBoxes));
}

TEST(StaticSpatialIndexTests, query) {
  auto index = createIndex();
  std::vector<std::size_t> queryResults;
  index.query(40, 40, 60, 60, queryResults);
  std::vector<std::size_t> expectedIndexes = {6, 29, 31, 75};
  ASSERT_THAT(queryResults, t::UnorderedPointwise(t::Eq(), expectedIndexes));
}

TEST(StaticSpatialIndexTests, visitQuery) {
  auto index = createIndex();

  std::vector<std::size_t> queryResults;
  auto visitor = [&](std::size_t index) {
    queryResults.push_back(index);
    return true;
  };

  index.visitQuery(40, 40, 60, 60, visitor);
  std::vector<std::size_t> expectedIndexes = {6, 29, 31, 75};
  ASSERT_THAT(queryResults, t::UnorderedPointwise(t::Eq(), expectedIndexes));
}

TEST(StaticSpatialIndexTests, visitQuery_stops_early) {
  auto index = createIndex();

  std::vector<std::size_t> queryResults;
  auto visitor = [&](std::size_t index) {
    queryResults.push_back(index);
    return queryResults.size() != 2;
  };

  index.visitQuery(40, 40, 60, 60, visitor);
  std::vector<std::size_t> expectedIndexes = {6, 29, 31, 75};
  ASSERT_THAT(queryResults, t::SizeIs(2));
  ASSERT_THAT(expectedIndexes, t::IsSupersetOf(queryResults));
}

TEST(StaticSpatialIndexTests, visitItemBoxes) {
  auto index = createIndex();

  std::vector<std::size_t> indexes;
  auto visitor = [&](std::size_t index, double, double, double, double) {
    indexes.push_back(index);
    return true;
  };

  index.visitItemBoxes(visitor);

  std::vector<std::size_t> expectedIndexes;
  expectedIndexes.reserve(100);
  for (std::size_t i = 0; i < 100; ++i) {
    expectedIndexes.push_back(i);
  }

  ASSERT_THAT(indexes, t::WhenSorted(t::ContainerEq(expectedIndexes)));
}
int main(int argc, char **argv) {
  t::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
