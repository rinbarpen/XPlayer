#pragma once

#include <fmt/core.h>

#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#define LOG_DEBUG(fmt, ...) \
  log(LDEBUG, time(0), __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) \
  log(LINFO, time(0), __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) \
  log(LWARN, time(0), __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) \
  log(LERROR, time(0), __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...) \
  log(LFATAL, time(0), __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__)

enum LogLevel {
  LNONE = 0,
  LDEBUG,
  LINFO,
  LWARN,
  LERROR,
  LFATAL,
};

template <typename... Args>
static void log(LogLevel level, time_t timestamp, const char *function,
                int line, fmt::string_view fmt, Args &&...args) {
  const char *prefix;
  switch (level) {
    case LDEBUG:
      prefix = "\033[34m[DEBUG]\t";
      break;
    case LINFO:
      prefix = "\033[32m[INFO]\t";
      break;
    case LWARN:
      prefix = "\033[33m[WARN]\t";
      break;
    case LERROR:
      prefix = "\033[31;1m[ERROR]\t";
      break;
    case LFATAL:
      prefix = "\033[31;2m[FATAL]\t";
      break;
    default:
      prefix = "[UNKNOWN]\t";
      break;
  }
  fmt::print(stdout, prefix);

  struct tm tm;
  localtime_r(&timestamp, &tm);
  char buffer[20]{};
  strftime(buffer, 20, "%Y-%m-%d %H:%M:%S", &tm);
  fmt::print(stdout, buffer);


  fmt::print(stdout, "\t{}:{}\t", function, line);
  fmt::print(stdout, fmt, std::move(args)...);
  fmt::println(stdout, "\033[0m");
}
