
add_library(PY32F071_Driver INTERFACE)
target_include_directories(PY32F071_Driver INTERFACE Inc)
target_link_libraries(PY32F071_Driver INTERFACE CMSIS)
target_sources(PY32F071_Driver INTERFACE
    Src/py32f071_ll_rcc.c
    Src/py32f071_ll_pwr.c
    Src/py32f071_ll_dma.c
    Src/py32f071_ll_gpio.c
    Src/py32f071_ll_utils.c
    Src/py32f071_ll_adc.c
    Src/py32f071_ll_comp.c
    Src/py32f071_ll_crc.c
    Src/py32f071_ll_dac.c
    Src/py32f071_ll_exti.c
    Src/py32f071_ll_i2c.c
    Src/py32f071_ll_rtc.c
    Src/py32f071_ll_spi.c
    Src/py32f071_ll_tim.c
    Src/py32f071_ll_lptim.c
    Src/py32f071_ll_usart.c
)
