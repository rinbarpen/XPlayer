#pragma once

#include "xplayer/common.h"
#include <stdexcept>
class Generic
{
public:
  virtual ~Generic() {}
};
template <typename T>
class ForwardGeneric : public Generic
{
public:
  ForwardGeneric(T begin) : value_(begin) {}
  ~ForwardGeneric() {}

  T current() const { return value_; }
  void next() { ++value_; }

private:
  T value_;
};
template <typename T>
class ReverseGeneric : public Generic
{
public:
  ReverseGeneric(T begin) : value_(begin) {}
  ~ReverseGeneric() {}

  T current() const { return value_; }
  void prev() { --value_; }

private:
  T value_;
};
template <typename T>
class RangeGeneric : public Generic
{
public:
  RangeGeneric(T begin, T end, bool isForward = true) : begin_(begin), end_(end) {
    ASSERT(begin < end);
    value_ = isForward ? begin : end - 1;
  }
  ~RangeGeneric() {}

  T current() const { return value_; }
  void prev() {
    if (value_ <= begin_)
      throw std::out_of_range("Value is out of left range");
    --value_; }
  void next() { ++value_; }

private:
  T value_;
  T begin_;
  T end_;
};
