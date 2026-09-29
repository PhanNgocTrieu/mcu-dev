cmake_minimum_required(VERSION 3.16)
project(connectivity-usb VERSION 0.1.0 LANGUAGES C CXX)

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
    src/registry.c
    src/policy.c
    src/storage.c
    src/transport.c
    src/adapter.c
    src/hotplug.c
    src/manager.c
)

add_library(connectivity_usb STATIC ${USB_C_SOURCES})
target_include_directories(connectivity_usb PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${UDEV_INCLUDE_DIRS}
    ${LIBUSB_INCLUDE_DIRS}
)
target_link_libraries(connectivity_usb PUBLIC ${UDEV_LIBRARIES} ${LIBUSB_LIBRARIES} pthread)

add_library(connectivity_usb_dbus STATIC src/dbus_sdbuspp.cpp)
target_include_directories(connectivity_usb_dbus PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SDBUSPP_INCLUDE_DIRS}
)
target_link_libraries(connectivity_usb_dbus PUBLIC connectivity_usb ${SDBUSPP_LIBRARIES})

add_executable(connectivity-usb src/main.c)
target_link_libraries(connectivity-usb PRIVATE connectivity_usb_dbus)
if(DEFINED ENV{USB_MANAGER_DEPS_PREFIX})
    set_target_properties(connectivity-usb PROPERTIES
        BUILD_RPATH "$ENV{USB_MANAGER_DEPS_PREFIX}/usr/lib/x86_64-linux-gnu")
endif()

add_executable(connectivity-usb-selftest tools/selftest.c)
target_link_libraries(connectivity-usb-selftest PRIVATE connectivity_usb)

enable_testing()
add_test(NAME connectivity-usb-selftest COMMAND connectivity-usb-selftest)

include(GNUInstallDirs)
install(TARGETS connectivity-usb RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES systemd/connectivity-usb.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)
install(FILES udev/99-connectivity-usb.rules DESTINATION lib/udev/rules.d)
