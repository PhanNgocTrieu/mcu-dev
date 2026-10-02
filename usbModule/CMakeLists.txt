cmake_minimum_required(VERSION 3.16)
project(usb-module VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(USBMOD_BUILD_TESTS "Build usbmod-selftest" ON)

set(USBMOD_SOURCES
    src/classify.c
    src/aoa.c
    src/manager.c
)

if(USBDRV_SOURCE_DIR)
    set(USBDRV_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    add_subdirectory(${USBDRV_SOURCE_DIR} ${CMAKE_BINARY_DIR}/usbdrv)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(USBDRV REQUIRED usbdrv)
endif()

add_library(usbmod STATIC ${USBMOD_SOURCES})
target_include_directories(usbmod PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_options(usbmod PRIVATE -Wall -Wextra -Werror)
if(USBDRV_SOURCE_DIR)
    target_link_libraries(usbmod PUBLIC usbdrv)
else()
    target_include_directories(usbmod PUBLIC ${USBDRV_INCLUDE_DIRS})
    target_link_libraries(usbmod PUBLIC ${USBDRV_LIBRARIES})
endif()

add_executable(usb-module src/main.c)
target_link_libraries(usb-module PRIVATE usbmod)
target_compile_options(usb-module PRIVATE -Wall -Wextra -Werror)
if(USBDRV_SOURCE_DIR)
    set_target_properties(usb-module PROPERTIES BUILD_RPATH "${CMAKE_BINARY_DIR}/usbdrv")
endif()

include(GNUInstallDirs)
install(TARGETS usb-module RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES include/usbmod.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES systemd/usb-module.service DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system)

if(USBMOD_BUILD_TESTS)
    add_executable(usbmod-selftest tests/selftest.c)
    target_link_libraries(usbmod-selftest PRIVATE usbmod)
    target_compile_options(usbmod-selftest PRIVATE -Wall -Wextra -Werror)
    if(USBDRV_SOURCE_DIR)
        set_target_properties(usbmod-selftest PROPERTIES BUILD_RPATH "${CMAKE_BINARY_DIR}/usbdrv")
    endif()
    enable_testing()
    add_test(NAME usbmod-selftest COMMAND usbmod-selftest)
endif()
