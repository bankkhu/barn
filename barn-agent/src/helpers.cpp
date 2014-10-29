/*
 * Some general helper/utility functions for barn-agent.
 */

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include <boost/algorithm/string.hpp>

#include "helpers.h"

using namespace std;

/**/
const vector<string> split(string str, char delim) {
  vector<string> tokens;
  boost::split(tokens, str, boost::is_any_of(string(1, delim)));
  return tokens;
}

/**/
const vector<string> prepend_each(vector<string> vec, string prefix) {
  vector<string> new_vec;

  for (string& el : vec)
    new_vec.push_back(prefix + el);

  return new_vec;
}


/*
 * Given two sorted vectors, returns the number of elements
 * in small not in big.
 */
int count_missing(const vector<string>& small, const vector<string>& big) {
  int missing = 0;

  for (auto sm = small.begin(), bg = big.begin();
       sm != small.end();) {
       if (bg == big.end() || *sm < *bg) {
         ++missing;
         ++sm;
       } else if (*sm == *bg) {
         ++sm;
         ++bg;
       } else {
         ++bg;
       }
  }
  return missing;
}

/*
 * Given two sorted vectors, finds the elements that are in both
 * A and B starting at the end. Stops at the first difference.
 *
 * Example:
 *      A = {1,2,3,4,5,6,7,8}
 *      B = {1,2,3,4,    7,8}
 * result = {7,8}
*/
vector<string> tail_intersection(const std::vector<string>& A, const std::vector<string>& B) {
  vector<string> result;

  for (auto a = A.rbegin(), b = B.rbegin();
       a != A.rend() && b != B.rend();
       ++a, ++b) {
       if (*a == *b)
         result.push_back(*a);
       else
         break;
  }
  reverse(result.begin(), result.end());

  return result;
}

