include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)
link_directories(/opt/vc/lib)

add_library(rameutil SHARED librameutil.c)
target_link_libraries(rameutil bcm_host EGL GLESv2 ${FT_LIBRARIES})
install(TARGETS rameutil DESTINATION lib)
