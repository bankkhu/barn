#include <vector>
#include "gtest/gtest.h"
#include "helpers.h"

using namespace std;

class HelpersTest : public ::testing::Test {
};

TEST_F(HelpersTest, ShipAll) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> expected      = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> actual = larger_than_gap(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(HelpersTest, ShipNone) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {};
  vector<string> expected      = {};
  vector<string> actual = larger_than_gap(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(HelpersTest, ShipOne) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"8"};
  vector<string> expected      = {"8"};
  vector<string> actual = larger_than_gap(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(HelpersTest, WeirdGap_ShipNone) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"7"};
  vector<string> expected      = {};
  vector<string> actual = larger_than_gap(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(HelpersTest, ShipMany) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"5", "6", "7", "8"};
  vector<string> expected      = {"5", "6", "7", "8"};
  vector<string> actual = larger_than_gap(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}

TEST_F(HelpersTest, CanonicalExample) {
  vector<string> local_files   = {"1", "2", "3", "4", "5", "6", "7", "8"};
  vector<string> missing_files = {"1", "2",           "5",      "7", "8"};
  vector<string> expected      = {"7", "8"};
  vector<string> actual = larger_than_gap(local_files, missing_files);
  EXPECT_EQ(expected, actual);
}



