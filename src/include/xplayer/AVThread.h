#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <string>
#include <thread>
#include <utility>

#include "Mutex.h"

class AVThread {
 public:
  AVThread(const std::string& name) : name_(name) {}
  virtual ~AVThread() {
    if (thread_.joinable()) thread_.join();
  }

  template <typename Fn, typename... Args>
  void dispatch(Fn&& f, Args&&... args) {
    // callback_ = [&]{
    //   std::forward<Fn>(f)(std::forward<Args>(args)...);
    // };
    // if (thread_.joinable()) thread_.join();
    thread_ = std::thread(std::forward<Fn>(f), std::forward<Args>(args)...);
  }

  void open() {
    opened_ = true;
    cond_.notify_all();
  }
  void close() { opened_ = false; }
  bool isOpening() const { return opened_; }

  void wait() {
    Mutex::ulock locker(mutex_);
    cond_.wait(locker, [this] { return isOpening(); });
  }
  void signal() { cond_.notify_one(); }

  void join() {
    close();
    if (thread_.joinable()) thread_.join();
  }

  std::string name() const { return name_; }

 protected:
  std::atomic_bool opened_;
  std::thread thread_;
  std::string name_;
  Mutex::type mutex_;
  std::condition_variable cond_;

  // std::function<void()> callback_;
};
