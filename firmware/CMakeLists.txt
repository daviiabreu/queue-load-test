cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk)
set(PICO_BOARD pico_w)
include(pico_sdk_import.cmake)

project(pico_telemetry C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_telemetry
    src/core/main.cpp
    src/sensors/adc_sensor.cpp
    src/sensors/gpio_sensor.cpp
    src/sensors/ultrasonic_sensor.cpp
    src/network/wifi_manager.cpp
    src/network/http_client.cpp
)

target_include_directories(pico_telemetry PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sensors
    ${CMAKE_CURRENT_SOURCE_DIR}/src/network
)

target_link_libraries(pico_telemetry
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    hardware_adc
    hardware_gpio
    hardware_irq
)

pico_enable_stdio_usb(pico_telemetry 1)
pico_enable_stdio_uart(pico_telemetry 0)

pico_add_extra_outputs(pico_telemetry)
