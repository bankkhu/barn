#include <vector>
#include "gtest/gtest.h"
#include "helpers.h"

using namespace std;

class HelpersTest : public ::testing::Test {
};

class TailIntersectionTest : public HelpersTest {
};

TEST_F(TailIntersectionTest, ShipAll) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> expected      = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> actual = tail_intersection(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(TailIntersectionTest, ShipNone) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {};
  vector<string> expected      = {};
  vector<string> actual = tail_intersection(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(TailIntersectionTest, ShipOne) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"8"};
  vector<string> expected      = {"8"};
  vector<string> actual = tail_intersection(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(TailIntersectionTest, WeirdGap_ShipNone) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"7"};
  vector<string> expected      = {};
  vector<string> actual = tail_intersection(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(TailIntersectionTest, ShipMany) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"5", "6", "7", "8"};
  vector<string> expected      = {"5", "6", "7", "8"};
  vector<string> actual = tail_intersection(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(TailIntersectionTest, CanonicalExample) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"1", "2",           "5",      "7", "8"};
  vector<string> expected      = {"7", "8"};
  vector<string> actual = tail_intersection(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}


class CountMissingTest : public HelpersTest {
};

TEST_F(CountMissingTest, Empty) {
  vector<string> small = {};
  vector<string> big   = {};
  int actual = count_missing(small, big);
  EXPECT_EQ(0, actual);
}

TEST_F(CountMissingTest, BigEmpty) {
  vector<string> small = {"A", "B", "C"};
  vector<string> big   = {};
  int actual = count_missing(small, big);
  EXPECT_EQ(3, actual);
}

TEST_F(CountMissingTest, SmallEmpty) {
  vector<string> small = {};
  vector<string> big   = {"A", "B", "C"};
  int actual = count_missing(small, big);
  EXPECT_EQ(0, actual);
}

TEST_F(CountMissingTest, Normal) {
  vector<string> small = {"C", "D"};
  vector<string> big   = {"A", "B", "D", "E", "F"};
  int actual = count_missing(small, big);
  EXPECT_EQ(1, actual);
}

TEST_F(CountMissingTest, Ends) {
  vector<string> small = {     "B", "C"};
  vector<string> big   = {"A", "B", "C"};
  int actual = count_missing(small, big);
  EXPECT_EQ(0, actual);

  small = {"A", "B"     };
  big   = {"A", "B", "C"};
  actual = count_missing(small, big);
  EXPECT_EQ(0, actual);

  small = {     "B", "C"};
  big   = {"A", "B"     };
  actual = count_missing(small, big);
  EXPECT_EQ(1, actual);

  small = {"A", "B"     };
  big   = {     "B", "C"};
  actual = count_missing(small, big);
  EXPECT_EQ(1, actual);
}

TEST_F(CountMissingTest, Full) {
  vector<string> small = {"1", "2"};
  vector<string> big   = {"A", "B", "C", "D", "E"};
  int actual = count_missing(small, big);
  EXPECT_EQ(2, actual);
}

TEST_F(CountMissingTest, BarnCase) {
  vector<string> small = { "@40001", "@40002", "@40003", "@40004"};
  vector<string> big =   {                     "@40003", "@40004", "@40005" };
  int actual = count_missing(small, big);
  EXPECT_EQ(2, actual);
}
