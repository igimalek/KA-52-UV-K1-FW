
add_library(CherryUSB INTERFACE)
target_include_directories(CherryUSB INTERFACE 
    core 
    port 
    common
    class/cdc
)
target_sources(CherryUSB INTERFACE
    core/usbd_core.c
    port/usb_dc_py32.c
    class/cdc/usbd_cdc.c
)
target_link_libraries(CherryUSB INTERFACE CMSIS)
