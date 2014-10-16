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

int count_missing(const std::vector<std::string>& small, const std::vector<std::string>& big);
std::vector<std::string> tail_intersection(const std::vector<std::string>& A, const std::vector<std::string>& B);

/*
 * This tries to be a poor man's Scala's scalaz's Validation class.
 */
template <typename T>
using Validation = typename boost::variant<BarnError, T>;

// Only here to make gmock mocks compile.
template<typename T>
std::ostream& operator <<(std::ostream& out, Validation<T> const& rhs) {
  return out;
}

/*
 * Similar to Validation in scalaz.
 */
template<typename T> bool isFailure(Validation<T> v) {
  BarnError* error_value = boost::get<BarnError>(&v);
  return error_value != 0;
}

template<typename T> T get(Validation<T> v) {
  assert(!isFailure(v));
  T* success_value = boost::get<T>(&v);
  return *success_value;
}

template<typename T> BarnError error(Validation<T> v) {
  BarnError* error_value = boost::get<BarnError>(&v);
  return *error_value;
}

#endif
