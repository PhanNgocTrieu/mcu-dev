cmake_minimum_required(VERSION 3.16)
project(hupi-module-usb LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
enable_testing()

add_subdirectory(modules/common)
add_subdirectory(modules/usb-driver-userlayer)
add_subdirectory(modules/libhu-aa)
add_subdirectory(modules/libhu-carplay)
add_subdirectory(modules/usb-man)
add_subdirectory(rasp4/build-demo/apps/cluster)
add_subdirectory(rasp4/build-demo/apps/usb-panel)

add_executable(hupi-itest modules/tests/itest.c)
target_link_libraries(hupi-itest PRIVATE hupi_wire)
target_compile_options(hupi-itest PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
target_compile_definitions(hupi-itest PRIVATE
    DRIVERD_PATH="${CMAKE_BINARY_DIR}/modules/usb-driver-userlayer/usb-driverd"
    MANAGERD_PATH="${CMAKE_BINARY_DIR}/modules/usb-man/usb-managerd"
)
add_test(NAME hupi-itest COMMAND hupi-itest)
