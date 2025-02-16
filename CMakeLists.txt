cmake_minimum_required(VERSION 3.13)

set(PROGRAM_NAME picow_webapp)
set(PICO_BOARD pico_w)

if(NOT $ENV{PICO_SDK_PATH})
   message(" you must have pico sdk and environment PICO_SDK_PATH defined")
endif()
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

message(PICO_SDK_PATH="${PICO_SDK_PATH}")

include(pico_sdk_import.cmake)

project(${PROGRAM_NAME})

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()


message("Running makefsdata python script")
execute_process(COMMAND
    python3 makefsdata.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)

add_executable(${PROGRAM_NAME}
        picow_web.c
        dhcpserver/dhcpserver.c
        dnsserver/dnsserver.c
        cgi.c
        ssi.c
        post_handler.c
        ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns.c
        ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_domain.c
        ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_out.c
        mdns_example.c
        )

#target_compile_options(${PROGRAM_NAME} PUBLIC -Werror -Wall -Wextra -Wnull-dereference -Wno-unused-parameter -Wno-null-dereference -ggdb3 -O3)

target_compile_options(${PROGRAM_NAME} PUBLIC -ggdb3 -O0 -UNDEBUG)

string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage -Wl,--cref")

target_include_directories(${PROGRAM_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        ${CMAKE_CURRENT_LIST_DIR}/dhcpserver
        ${CMAKE_CURRENT_LIST_DIR}/dnsserver
        )

#pass some cmake variables to the program, warning some may be case sensitive.
target_compile_definitions(${PROGRAM_NAME} PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    HOSTNAME=\"${HOSTNAME}\"
    )

target_link_libraries(${PROGRAM_NAME}
        pico_cyw43_arch_lwip_threadsafe_background
        pico_stdlib
        pico_lwip_http
        hardware_adc
        pico_rand
        hardware_rtc
        pico_lwip_sntp
        )

pico_add_extra_outputs(${PROGRAM_NAME})


pico_enable_stdio_usb(${PROGRAM_NAME} FALSE)
pico_enable_stdio_uart(${PROGRAM_NAME} TRUE)
