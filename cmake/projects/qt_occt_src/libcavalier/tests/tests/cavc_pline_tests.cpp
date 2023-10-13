#include "c_api_test_helpers.hpp"
#include "cavaliercontours.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <vector>

namespace t = testing;

class cavc_plineTests : public t::Test {
protected:
  void SetUp() override;

  void TearDown() override;

  std::vector<cavc_vertex> pline1Vertexes;
  cavc_pline *pline1 = nullptr;
  cavc_pline *pline2 = nullptr;
  std::size_t origPline1Size = 0;
  uint32_t initialPline1Size() { return static_cast<uint32_t>(origPline1Size); }
};

void cavc_plineTests::SetUp() {
  pline1Vertexes = {{1, 2, 0.1}, {33, 3, 0.2}, {34, 35, 0.3}, {2, 36, 0.4}};
  origPline1Size = pline1Vertexes.size();
  pline1 = cavc_pline_new(&pline1Vertexes[0], initialPline1Size(), 0);
  pline2 = cavc_pline_new(nullptr, 0, 1);
}

void cavc_plineTests::TearDown() {
  cavc_pline_delete(pline1);
  cavc_pline_delete(pline2);
}

TEST_F(cavc_plineTests, cavc_pline_new) {

  // test capacity
  EXPECT_EQ(cavc_pline_capacity(pline1), initialPline1Size());

  // test is_closed
  EXPECT_EQ(cavc_pline_is_closed(pline1), 0);

  // test vertex_count
  auto pline1Count = cavc_pline_vertex_count(pline1);
  ASSERT_EQ(pline1Count, initialPline1Size());

  // test vertex_data
  std::vector<cavc_vertex> read_vertexes(pline1Count);
  cavc_pline_vertex_data(pline1, &read_vertexes[0]);
  ASSERT_THAT(pline1Vertexes, t::Pointwise(VertexEqual(), read_vertexes));

  // test on empty pline
  // test capacity
  EXPECT_EQ(cavc_pline_capacity(pline2), 0);

  // test is_closed
  EXPECT_EQ(cavc_pline_is_closed(pline2), 1);

  // test vertex_count
  auto pline2Count = cavc_pline_vertex_count(pline2);
  ASSERT_EQ(pline2Count, 0);

  // test vertex_data
  cavc_pline_vertex_data(pline2, &read_vertexes[0]);
  // nothing should have been written to the buffer
  ASSERT_THAT(read_vertexes, t::Pointwise(VertexEqual(), pline1Vertexes));
}

TEST_F(cavc_plineTests, cavc_pline_set_capacity) {
  // setting capacity less than current does nothing
  cavc_pline_set_capacity(pline1, 1);
  ASSERT_EQ(cavc_pline_capacity(pline1), 4);

  cavc_pline_set_capacity(pline1, 11);
  ASSERT_EQ(cavc_pline_capacity(pline1), 11);
}

TEST_F(cavc_plineTests, cavc_pline_set_vertex_data) {
  cavc_pline_set_vertex_data(pline2, &pline1Vertexes[0], initialPline1Size());
  ASSERT_EQ(cavc_pline_vertex_count(pline2), initialPline1Size());

  std::vector<cavc_vertex> readVertexes(initialPline1Size());
  cavc_pline_vertex_data(pline2, &readVertexes[0]);
  ASSERT_THAT(readVertexes, t::Pointwise(VertexEqual(), pline1Vertexes));
}

TEST_F(cavc_plineTests, cavc_pline_add_vertex) {
  cavc_vertex v{555, 666, 0.777};
  cavc_pline_add_vertex(pline1, v);
  ASSERT_EQ(cavc_pline_vertex_count(pline1), initialPline1Size() + 1);

  std::vector<cavc_vertex> readVertexes(initialPline1Size() + 1);
  cavc_pline_vertex_data(pline1, &readVertexes[0]);
  pline1Vertexes.push_back(v);
  ASSERT_THAT(readVertexes, t::Pointwise(VertexEqual(), pline1Vertexes));

  cavc_pline_add_vertex(pline2, v);
  ASSERT_EQ(cavc_pline_vertex_count(pline2), 1);

  readVertexes.resize(1);
  cavc_pline_vertex_data(pline2, &readVertexes[0]);
  ASSERT_THAT(readVertexes, t::Pointwise(VertexEqual(), {v}));
}

TEST_F(cavc_plineTests, cavc_pline_remove_range) {
  // remove first vertex
  cavc_pline_remove_range(pline1, 0, 1);
  ASSERT_EQ(cavc_pline_vertex_count(pline1), initialPline1Size() - 1);

  std::vector<cavc_vertex> readVertexes(initialPline1Size() - 1);
  cavc_pline_vertex_data(pline1, &readVertexes[0]);
  pline1Vertexes.erase(pline1Vertexes.begin());
  ASSERT_THAT(readVertexes, t::Pointwise(VertexEqual(), pline1Vertexes));

  // remove 2nd and 3rd vertex
  cavc_pline_remove_range(pline1, 1, 2);
  ASSERT_EQ(cavc_pline_vertex_count(pline1), initialPline1Size() - 3);
  readVertexes.resize(1);
  cavc_pline_vertex_data(pline1, &readVertexes[0]);
  pline1Vertexes.erase(pline1Vertexes.begin() + 1, pline1Vertexes.begin() + 3);
  ASSERT_THAT(readVertexes, t::Pointwise(VertexEqual(), pline1Vertexes));

  // remove last vertex
  cavc_pline_remove_range(pline1, 0, 1);
  ASSERT_EQ(cavc_pline_vertex_count(pline1), initialPline1Size() - 4);

  readVertexes.resize(10);
  std::fill(readVertexes.begin(), readVertexes.end(), cavc_vertex{-1, -2, -3});
  auto copy = readVertexes;
  cavc_pline_vertex_data(pline1, &readVertexes[0]);
  // nothing should have been written to the buffer
  ASSERT_THAT(readVertexes, t::Pointwise(VertexEqual(), copy));
}

TEST_F(cavc_plineTests, cavc_pline_clear) {
  cavc_pline_clear(pline1);
  ASSERT_EQ(cavc_pline_vertex_count(pline1), 0);

  cavc_pline_clear(pline2);
  ASSERT_EQ(cavc_pline_vertex_count(pline2), 0);
}

int main(int argc, char **argv) {
  t::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
