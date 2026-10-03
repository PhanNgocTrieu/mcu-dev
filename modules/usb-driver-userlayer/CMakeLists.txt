cmake_minimum_required(VERSION 3.16)
project(usb-driver VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(USBDRV_BUILD_TESTS "Build usb-driver selftest" ON)

get_filename_component(HUPI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT TARGET hupi_wire)
    add_subdirectory(${HUPI_ROOT}/common ${CMAKE_CURRENT_BINARY_DIR}/hupi-common)
endif()

add_library(usbdrv SHARED
    src/enum.c
    src/uevent.c
    src/usbfs.c
)
target_include_directories(usbdrv PUBLIC include)
target_compile_options(usbdrv PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
set_target_properties(usbdrv PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION 1)

add_executable(usb-driverd src/driverd.c)
target_link_libraries(usb-driverd PRIVATE usbdrv hupi_wire)
target_compile_options(usb-driverd PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)

include(GNUInstallDirs)
install(TARGETS usbdrv LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS usb-driverd RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/usbdrv.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES systemd/usb-driverd.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)

if(USBDRV_BUILD_TESTS)
    add_executable(usbdrv-selftest tests/selftest.c)
    target_link_libraries(usbdrv-selftest PRIVATE usbdrv hupi_wire)
    target_compile_options(usbdrv-selftest PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
    enable_testing()
    add_test(NAME usbdrv-selftest COMMAND usbdrv-selftest)
endif()
