project(rameplayer-utils C)
cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
pkg_check_modules(FT freetype2)

set(COMPILE_DEFINITIONS -Werror -Wall -O3)
include_directories(${FT_INCLUDE_DIRS})

add_subdirectory(ramefbcp)
add_subdirectory(rametext)
add_subdirectory(rameclock)
