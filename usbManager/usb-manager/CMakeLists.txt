cmake_minimum_required(VERSION 3.16)
project(usb-manager VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optional local deps prefix (extracted -dev packages without sudo)
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
pkg_check_modules(SYSTEMD REQUIRED libsystemd)

add_library(usb_manager_core
  src/util/Log.cpp
  src/classify/DeviceClassifier.cpp
  src/hotplug/UdevMonitor.cpp
  src/registry/DeviceRegistry.cpp
  src/policy/ConnectionPolicy.cpp
  src/aoap/TransportProber.cpp
  src/adapters/DemoAasdkSession.cpp
  src/adapters/AndroidAutoAdapter.cpp
  src/adapters/CarPlayStubAdapter.cpp
  src/session/SessionOrchestrator.cpp
  src/ipc/UsbDbusService.cpp
)

target_include_directories(usb_manager_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${UDEV_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
    ${SYSTEMD_INCLUDE_DIRS}
)

target_link_libraries(usb_manager_core
  PUBLIC
    ${UDEV_LIBRARIES}
    ${LIBUSB_LIBRARIES}
    ${SYSTEMD_LIBRARIES}
    pthread
)

add_executable(usb-manager src/main.cpp)
target_link_libraries(usb-manager PRIVATE usb_manager_core)

add_executable(usb-udev-lab tools/usb_udev_lab.cpp)
target_link_libraries(usb-udev-lab PRIVATE usb_manager_core)

add_executable(usb-manager-selftest tools/usb_manager_selftest.cpp)
target_link_libraries(usb-manager-selftest PRIVATE usb_manager_core)

enable_testing()
add_test(NAME usb-manager-selftest COMMAND usb-manager-selftest)

include(GNUInstallDirs)
install(TARGETS usb-manager usb-udev-lab
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES systemd/usb-manager.service
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)
install(FILES udev/99-connectivity-usb.rules
  DESTINATION lib/udev/rules.d)
