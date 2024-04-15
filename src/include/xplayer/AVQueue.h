#pragma once

#include <atomic>
#include <mutex>
#include <queue>

#include "xplayer/FFmpegUtil.h"
#include "xplayer/noncopyable.h"

template <typename T>
class AVQueue : public noncopyable {
 public:
  AVQueue() = default;
  virtual ~AVQueue() = default;

  void open() { opened_ = true; }
  void close() { opened_ = false; }

  bool push(T&& x) {
    if (!opened_) return false;
    std::lock_guard<std::mutex> locker(mutex_);
    data_.emplace(std::move(x));
    return true;
  }
  bool push(const T& x) {
    if (!opened_) return false;
    std::lock_guard<std::mutex> locker(mutex_);
    data_.emplace(x);
    return true;
  }
  bool pop() {
    if (!opened_) return false;
    std::lock_guard<std::mutex> locker(mutex_);
    if (data_.empty()) return false;
    data_.pop();
    return true;
  }
  bool pop(T& x) {
    if (!opened_) return false;
    std::lock_guard<std::mutex> locker(mutex_);
    if (data_.empty()) return false;
    x = std::move(data_.front());
    data_.pop();
    return true;
  }

  bool isEmpty() const {
    std::lock_guard<std::mutex> locker(mutex_);
    return data_.empty();
  }
  size_t size() const {
    std::lock_guard<std::mutex> locker(mutex_);
    return data_.size();
  }

  bool isOpening() const { return opened_; }

 protected:
  // std::atomic_bool opened_{false};
  bool opened_{false};
  std::queue<T> data_;
  mutable std::mutex mutex_;
};


class AVFrameQueue : public AVQueue<AVFrame*> {
 public:
  ~AVFrameQueue() {
    this->clear();
  }
  void clear() {
    AVFrame* pFrame;
    while (this->pop(pFrame)) {
      av_frame_unref(pFrame);
    }
  }
};
class AVPacketQueue : public AVQueue<AVPacketPtr> {
 public:
  ~AVPacketQueue() {
    this->clear();
  }
  void clear() {
    std::lock_guard<std::mutex> locker(mutex_);
    AVPacketPtr pPacket;
    while (this->pop(pPacket)) {
    }
  }
};
