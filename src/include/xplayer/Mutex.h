#pragma once

#include <mutex>

class Mutex{
public:
  using type = std::mutex;
  using lock = std::lock_guard<type>;
  using ulock = std::unique_lock<type>;
};
