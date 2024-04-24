#pragma once

#include <atomic>
#include <chrono>
#include <list>
#include <condition_variable>

#include "libavcodec/packet.h"
#include "xplayer/FFmpegUtil.h"
#include "xplayer/Mutex.h"

template <typename T>
class AVQueue{
public:
  explicit AVQueue(size_t maxSize) : max_size_(maxSize) {}
  virtual ~AVQueue() { this->clear(); }

  void open() { opened_ = true; }
  void close() { opened_ = false; }

  bool push(const T& x) {
    if (!opened_) return false;

    wait();
    Mutex::lock locker(mutex_);
    data_.emplace_back(std::move(x));
    return true;
  }
  bool push(T&& x) {
    if (!opened_) return false;

    wait();
    Mutex::lock locker(mutex_);
    data_.emplace_back(std::move(x));
    return true;
  }
  bool pop(T& x) {
    if (!opened_) return false;

    Mutex::lock locker(mutex_);
    if (data_.empty()) return false;
    x = std::move(data_.front());
    data_.pop_front();
    if (data_.size() < max_size_ / 5)
      signal();
    return true;
  }

  bool isEmpty() const {
    Mutex::lock locker(mutex_);
    return data_.empty();
  }
  bool isFull() const {
    Mutex::lock locker(mutex_);
    return data_.size() >= max_size_;
  }
  size_t size() const {
    Mutex::lock locker(mutex_);
    return data_.size();
  }
  size_t maxSize() const { return max_size_; }
  void clear() {
    Mutex::lock locker(mutex_);
    data_.clear();
    signal();
  }
  void flush() { clear(); }

  bool isOpened() const { return opened_; }

  int seq() const { return seq_; }
  void step2nextSeq() { seq_++; }

  void wait() {
    Mutex::ulock locker(wait_mutex_);
    cond_.wait(locker, [this]{
      return (data_.size() < max_size_);
    });
  }
  void waitFor(int64_t ms) {
    Mutex::ulock locker(wait_mutex_);
    cond_.wait_for(locker, std::chrono::milliseconds(ms), [this]{
      return (data_.size() < max_size_);
    });
  }
  void signal() {
    cond_.notify_all();
  }

protected:
  std::atomic_bool opened_{false};
  std::list<T> data_;
  mutable Mutex::type mutex_;
  const size_t max_size_;

  Mutex::type wait_mutex_;
  std::condition_variable cond_;

  int seq_{0};
};

class AVPacketQueue : public AVQueue<AVPacketPtr> {
 public:
  explicit AVPacketQueue(size_t maxSize)
    : AVQueue<AVPacketPtr>(maxSize) {}
  ~AVPacketQueue() { this->clear(); }
};

class AVFrameQueue : public AVQueue<AVFramePtr> {
 public:
  explicit AVFrameQueue(size_t maxSize)
      : AVQueue<AVFramePtr>(maxSize) {}
  ~AVFrameQueue() { this->clear(); }
};
