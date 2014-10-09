#ifndef HELPERS_H
#define HELPERS_H

#include <vector>
#include <string>
#include <boost/lambda/lambda.hpp>
#include <boost/variant.hpp>

// Types
typedef std::string FileName;
typedef std::vector<FileName> FileNameList;
typedef std::string BarnError;


const std::vector<std::string> split(std::string str, char delim);
const std::vector<std::string> prepend_each(std::vector<std::string> vec, std::string prefix);
int count_missing(std::vector<std::string> small, std::vector<std::string> big);

/*
  Strange name for a function, so here's what it does.
  Given two sorted lists A and B, finds the set difference C = A \ B
  and returns every element in B except the ones smaller than the smallest
  element in C.

  Example:
  A = {1,2,3,4,5,6,7,8}
  B = {1,2,3,4,    7,8}
  C = A \ B = {5,6}
  D = {x | x ∈ B && x > max(C)}   // result = {7,8}
*/

// TODO: rename larger_than_gap to tail_intersection.
template<typename T>
std::vector<T> larger_than_gap(const std::vector<T> A, const std::vector<T> B) {
  using boost::lambda::_1;

  std::vector<T> C;

  set_difference(A.begin(), A.end(), B.begin(), B.end()
               , back_inserter(C));

  std::vector<T> D(B);

  if(!C.empty())
      D.erase(remove_if(D.begin(), D.end(), _1 <= C.back())
            , D.end());

  return D;
}


/*
 * This tries to be a poor man's Scala's scalaz's Validation class.
 */
template <typename T>
using Validation = typename boost::variant<BarnError, T>;

/*
 * Similar to Validation::fold on scalaz.
 * Find more here: https://github.com/scalaz/scalaz/blob/scalaz-seven/core/src/main/scala/scalaz/Validation.scala#L56
 */
template<typename T, typename SuccessFunc, typename FailureFunc>
void fold(Validation<T> v,
          SuccessFunc success,
          FailureFunc failure) {

  T* success_value = boost::get<T>(&v);
  BarnError* error_value = boost::get<BarnError>(&v);

  if(success_value != 0)
    success(*success_value);
  else
    failure(*error_value);
}

#endif
