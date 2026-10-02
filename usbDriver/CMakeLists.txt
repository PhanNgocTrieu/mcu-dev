cmake_minimum_required(VERSION 3.16)
project(usbdrv VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(USBDRV_BUILD_TESTS "Build usbdrv-selftest" ON)

add_library(usbdrv SHARED
    src/enum.c
    src/uevent.c
    src/usbfs.c
)
target_include_directories(usbdrv
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_options(usbdrv PRIVATE -Wall -Wextra -Werror)
set_target_properties(usbdrv PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 0
)

include(GNUInstallDirs)
install(TARGETS usbdrv LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES include/usbdrv.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
configure_file(usbdrv.pc.in usbdrv.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/usbdrv.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(USBDRV_BUILD_TESTS)
    add_executable(usbdrv-selftest tests/selftest.c)
    target_link_libraries(usbdrv-selftest PRIVATE usbdrv)
    target_compile_options(usbdrv-selftest PRIVATE -Wall -Wextra -Werror)
    enable_testing()
    add_test(NAME usbdrv-selftest COMMAND usbdrv-selftest)
endif()
