#pragma once

#include <string>

namespace usb_manager {

enum class LogLevel { Debug, Info, Warn, Error };

void logSetLevel(LogLevel level);
void logWrite(LogLevel level, const char* file, int line, const std::string& msg);

}  // namespace usb_manager

#define USB_LOG_DEBUG(msg) \
  ::usb_manager::logWrite(::usb_manager::LogLevel::Debug, __FILE__, __LINE__, msg)
#define USB_LOG_INFO(msg) \
  ::usb_manager::logWrite(::usb_manager::LogLevel::Info, __FILE__, __LINE__, msg)
#define USB_LOG_WARN(msg) \
  ::usb_manager::logWrite(::usb_manager::LogLevel::Warn, __FILE__, __LINE__, msg)
#define USB_LOG_ERROR(msg) \
  ::usb_manager::logWrite(::usb_manager::LogLevel::Error, __FILE__, __LINE__, msg)
