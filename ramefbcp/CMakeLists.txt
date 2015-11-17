cmake_minimum_required(VERSION 2.8)

SET(COMPILE_DEFINITIONS -Werror -Wall -O3)

include_directories(/usr/include/freetype2)

include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)

link_directories(/opt/vc/lib)

add_executable(ramefbcp main.c debug.c infodisplay.c ttf.c input.c)

target_link_libraries(ramefbcp bcm_host freetype)
