cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig)
pkg_check_modules(FT freetype2)

SET(COMPILE_DEFINITIONS -Werror -Wall -O3)

add_definitions(${COMPILE_DEFINITIONS})

include_directories(${FT_INCLUDE_DIRS})

include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)
link_directories(/opt/vc/lib)

add_executable(ramefbcp main.c debug.c infodisplay.c ttf.c input.c)
target_link_libraries(ramefbcp bcm_host ${FT_LIBRARIES})

install(TARGETS ramefbcp DESTINATION bin)
install(FILES ramefbcp.ttf DESTINATION share/fonts/TTF)
