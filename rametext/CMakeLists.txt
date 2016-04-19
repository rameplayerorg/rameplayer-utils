include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)
link_directories(/opt/vc/lib)

include_directories(../librameutil)

add_executable(rametext main.c)
target_link_libraries(rametext rameutil bcm_host EGL GLESv2)
install(TARGETS rametext DESTINATION bin)
