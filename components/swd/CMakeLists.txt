idf_component_register(
    SRCS
        "swd.c"
        "swd_dp.c"
        "swd_ap.c"
        "swd_core.c"
        "swd_debug.c"
        "stm32f1_flash.c"
        "swd_test.c"

    INCLUDE_DIRS
        "include"

    REQUIRES
        esp_driver_spi

    PRIV_REQUIRES
        esp_driver_gpio
        freertos
)

# 0 = clean UI, 1 = raw SWD/Flash diagnostic output
target_compile_definitions(${COMPONENT_LIB} PRIVATE SWD_VERBOSE=0)
