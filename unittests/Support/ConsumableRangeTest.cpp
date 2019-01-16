#include "hermes/Support/ConsumableRange.h"

#include <vector>

#include "gtest/gtest.h"

using namespace hermes;

namespace {

TEST(ConsumableRangeTest, Empty) {
  std::vector<int> v;
  ConsumableRange<std::vector<int>::iterator> empty(v.begin(), v.end());
  EXPECT_FALSE(empty.hasNext());
  EXPECT_EQ(0, empty.size());
}

TEST(ConsumableRangeTest, Simple) {
  std::vector<int> v{101, 202};
  ConsumableRange<std::vector<int>::iterator> r(v.begin(), v.end());
  EXPECT_TRUE(r.hasNext());
  EXPECT_EQ(2, r.size());

  EXPECT_EQ(101, r.peek());
  EXPECT_EQ(2, r.size());
  EXPECT_EQ(101, r.next());
  EXPECT_EQ(1, r.size());

  EXPECT_TRUE(r.hasNext());
  EXPECT_EQ(202, r.peek());
  EXPECT_EQ(1, r.size());
  EXPECT_EQ(202, r.next());
  EXPECT_EQ(0, r.size());

  EXPECT_FALSE(r.hasNext());
}

} // end anonymous namespace
