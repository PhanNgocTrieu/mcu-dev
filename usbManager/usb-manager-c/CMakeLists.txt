cmake_minimum_required(VERSION 3.16)
project(usb-manager-c VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(DEFINED ENV{USB_MANAGER_DEPS_PREFIX})
  list(APPEND CMAKE_PREFIX_PATH "$ENV{USB_MANAGER_DEPS_PREFIX}")
  include_directories("$ENV{USB_MANAGER_DEPS_PREFIX}/usr/include")
  link_directories("$ENV{USB_MANAGER_DEPS_PREFIX}/usr/lib/x86_64-linux-gnu")
  set(ENV{PKG_CONFIG_PATH}
      "$ENV{USB_MANAGER_DEPS_PREFIX}/usr/lib/x86_64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(UDEV REQUIRED libudev)
pkg_check_modules(LIBUSB REQUIRED libusb-1.0)
pkg_check_modules(SDBUSPP REQUIRED sdbus-c++)

set(USB_C_SOURCES
  src/types.c
  src/log.c
  src/classifier.c
  src/udev_monitor.c
  src/registry.c
  src/policy.c
  src/transport.c
  src/adapter.c
  src/orchestrator.c
)

add_library(usb_mgr_c STATIC ${USB_C_SOURCES})
target_include_directories(usb_mgr_c PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${UDEV_INCLUDE_DIRS}
  ${LIBUSB_INCLUDE_DIRS}
)
target_link_libraries(usb_mgr_c PUBLIC ${UDEV_LIBRARIES} ${LIBUSB_LIBRARIES} pthread)

add_library(usb_mgr_dbus STATIC src/dbus_sdbuspp.cpp)
target_include_directories(usb_mgr_dbus PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${SDBUSPP_INCLUDE_DIRS}
)
target_link_libraries(usb_mgr_dbus PUBLIC usb_mgr_c ${SDBUSPP_LIBRARIES})

add_executable(usb-manager-c src/main.c)
target_link_libraries(usb-manager-c PRIVATE usb_mgr_dbus)
if(DEFINED ENV{USB_MANAGER_DEPS_PREFIX})
  set_target_properties(usb-manager-c PROPERTIES
    BUILD_RPATH "$ENV{USB_MANAGER_DEPS_PREFIX}/usr/lib/x86_64-linux-gnu")
endif()

add_executable(usb-udev-lab-c tools/usb_udev_lab.c)
target_link_libraries(usb-udev-lab-c PRIVATE usb_mgr_c)

add_executable(usb-manager-c-selftest tools/selftest.c)
target_link_libraries(usb-manager-c-selftest PRIVATE usb_mgr_c)

enable_testing()
add_test(NAME usb-manager-c-selftest COMMAND usb-manager-c-selftest)
