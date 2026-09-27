idf_component_register(
    SRCS "src/auxbeam_light.c"
         "src/light_protocol.c"
         "src/checksum.c"
         "src/command_queue.c"
         "src/hal_uart_esp32.c"
    INCLUDE_DIRS "include"
    REQUIRES driver
)
