#pragma once

#include <chrono>
#include <cstdint>

class AVClock
{
public:
  AVClock() {
    last_ = std::chrono::steady_clock::now();
  }

  int64_t elapse() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - last_);
    last_ = now;
    return duration.count();
  }

private:
  std::chrono::steady_clock::time_point last_;
};

class AVSyncClock
{
public:
  AVSyncClock() = default;

  void setTs(int64_t ts) { curr_ts_ = ts; }
  int64_t current() const { return curr_ts_; }

  void reset() { curr_ts_ = 0; }

private:
  int64_t curr_ts_;
};
