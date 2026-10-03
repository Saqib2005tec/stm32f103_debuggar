idf_component_register(
    SRCS
        "main.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        ui
        swd

    EMBED_FILES
        "stm32f103_blink.bin"
)
