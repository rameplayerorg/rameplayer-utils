include_directories(/opt/vc/include)
include_directories(/opt/vc/include/interface/vcos/pthreads)
include_directories(/opt/vc/include/interface/vmcs_host)
include_directories(/opt/vc/include/interface/vmcs_host/linux)
link_directories(/opt/vc/lib)

include_directories(../librameutil)

add_executable(rameclock clock.c)
target_link_libraries(rameclock rameutil bcm_host EGL GLESv2 mosquitto)

add_executable(rameclock-button button.c)
target_link_libraries(rameclock-button mosquitto)

add_executable(rameclock-led led.c)
target_link_libraries(rameclock-led mosquitto)

install(TARGETS rameclock rameclock-button rameclock-led DESTINATION bin)
