cmake_minimum_required(VERSION 3.16)
project(hupi_common LANGUAGES C)

add_library(hupi_wire STATIC src/wire.c src/log.c)
target_include_directories(hupi_wire PUBLIC include)
target_compile_options(hupi_wire PRIVATE -Wall -Wextra -Werror -Wno-format-truncation)
target_link_libraries(hupi_wire PUBLIC pthread)
