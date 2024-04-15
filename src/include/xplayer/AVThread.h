#pragma once

#include <condition_variable>
#include <thread>
#include <atomic>
#include "FFmpegUtil.h"
#include "libavformat/avformat.h"

class AVThread{
public:
  AVThread() = default;
  virtual ~AVThread() = default;

  template <typename Fn, typename... Args>
  void dispatch(Fn&& f, Args&&... args) {
    thread_ = std::thread(std::forward<Fn>(f), std::forward<Args>(args)...);
  }

  virtual void run() = 0;

protected:
  std::thread thread_;
};

enum class ThreadStatus {
  NONE,
  READY,
  RUNNING,
  FINISHED,
  ABORT,
};

class AVDecodeThread : public AVThread {
public:
  AVDecodeThread() : AVThread() {}
  virtual ~AVDecodeThread() {
    if (thread_.joinable()) thread_.join();
  }

  virtual void run() {}  // NOT IMPLEMENTED

  void setFinished(bool finished) {
    finished_ = finished;
  }
  bool isFinished() const {
    return finished_;
  }
  void setRunning(bool running) {
    running_ = running;
  }
  bool isRunning() const {
    return running_;
  }

  void open() { running_ = true; cond_.notify_one(); }
  void close() { running_ = false; }

private:
  bool finished_{false};
  std::atomic_bool running_{false};
  std::mutex mutex_;
  std::condition_variable cond_;
};

// class AVSDLPlayerDecodeThread : public AVDecodeThread {
// public:
//   void run() override {

//   }
// };