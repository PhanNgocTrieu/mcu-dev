#include "usb_manager/util/Log.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <mutex>

namespace usb_manager {
namespace {

std::mutex gLogMu;
LogLevel gLevel = LogLevel::Info;

const char* levelName(LogLevel l) {
  switch (l) {
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info: return "INFO";
    case LogLevel::Warn: return "WARN";
    case LogLevel::Error: return "ERROR";
  }
  return "INFO";
}

}  // namespace

void logSetLevel(LogLevel level) { gLevel = level; }

void logWrite(LogLevel level, const char* file, int line, const std::string& msg) {
  if (static_cast<int>(level) < static_cast<int>(gLevel)) {
    return;
  }
  const char* base = file;
  for (const char* p = file; *p; ++p) {
    if (*p == '/') base = p + 1;
  }
  std::lock_guard<std::mutex> lock(gLogMu);
  std::cerr << "[" << levelName(level) << "] " << base << ":" << line << " " << msg
            << std::endl;
}

}  // namespace usb_manager
