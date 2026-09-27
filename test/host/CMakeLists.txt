cmake_minimum_required(VERSION 3.16)
project(auxbeam_light_host_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(test_runner
    ${COMPONENT_DIR}/src/checksum.c
    ${COMPONENT_DIR}/src/light_protocol.c
    ${COMPONENT_DIR}/src/command_queue.c
    ${COMPONENT_DIR}/src/auxbeam_light.c
    mock_hal_uart.c
    test_util.c
    test_checksum.c
    test_protocol.c
    test_queue.c
    test_integration.c
    main.c
)

target_include_directories(test_runner PRIVATE
    ${COMPONENT_DIR}/include
    ${COMPONENT_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/stubs
)

target_compile_options(test_runner PRIVATE -Wall -Wextra)

enable_testing()
add_test(NAME host_tests COMMAND test_runner)
