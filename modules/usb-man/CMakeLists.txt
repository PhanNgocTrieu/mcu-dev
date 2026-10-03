cmake_minimum_required(VERSION 3.16)
project(usb-man VERSION 1.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(USBMAN_BUILD_TESTS "Build usb-man selftest" ON)

get_filename_component(MODULES_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
if(NOT TARGET hupi_wire)
    add_subdirectory(${MODULES_ROOT}/common ${CMAKE_CURRENT_BINARY_DIR}/hupi-common)
endif()
if(NOT TARGET hu_aa)
    add_subdirectory(${MODULES_ROOT}/libhu-aa ${CMAKE_CURRENT_BINARY_DIR}/libhu-aa EXCLUDE_FROM_ALL)
endif()
if(NOT TARGET hu_carplay)
    add_subdirectory(${MODULES_ROOT}/libhu-carplay ${CMAKE_CURRENT_BINARY_DIR}/libhu-carplay EXCLUDE_FROM_ALL)
endif()

add_library(usbman STATIC
    src/classify.c
    src/session.c
    src/aoa.c
)
target_include_directories(usbman PUBLIC include)
target_link_libraries(usbman PUBLIC hupi_wire)
target_compile_options(usbman PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)

add_executable(usb-managerd src/managerd.c)
target_link_libraries(usb-managerd PRIVATE usbman hu_aa hu_carplay)
target_compile_options(usb-managerd PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)

add_executable(hupi-ctl src/ctl.c)
target_link_libraries(hupi-ctl PRIVATE hupi_wire)
target_compile_options(hupi-ctl PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)

include(GNUInstallDirs)
install(TARGETS usb-managerd hupi-ctl RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES systemd/usb-managerd.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)

if(USBMAN_BUILD_TESTS)
    add_executable(usbman-selftest tests/selftest.c)
    target_link_libraries(usbman-selftest PRIVATE usbman)
    target_compile_options(usbman-selftest PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
    enable_testing()
    add_test(NAME usbman-selftest COMMAND usbman-selftest)
endif()
