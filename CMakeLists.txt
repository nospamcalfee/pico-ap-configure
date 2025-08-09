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


#message("Running makefsdata python script")
#execute_process(COMMAND
#    python3 makefsdata.py
#    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
#)

add_executable(${PROGRAM_NAME}
        picow_web.c
        cgi.c
        ssi.c
        post_handler.c
        ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns.c
        ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_domain.c
        ${PICO_SDK_PATH}/lib/lwip/src/apps/mdns/mdns_out.c
        mdns_example.c
        flash_onboard.c
        ring_buffer.c
        flash_io.c
        relay_control.c
        find_local_ssid.c
        cdll.c
        picow_tcp_server.c
        picow_tcp_client.c
        picow_tcp_json.c
        cJSON.c
        json_handler.c
        )

#target_compile_options(${PROGRAM_NAME} PUBLIC -Werror -Wall -Wextra -Wnull-dereference -Wno-unused-parameter -Wno-null-dereference -ggdb3 -O3)

target_compile_options(${PROGRAM_NAME} PUBLIC -ggdb3 -O0)

string(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,--print-memory-usage")
#sometimes useful:  -Wl,--print-gc-sections
#sometimes useful:  -Wl,--cref

pico_set_linker_script(${PROGRAM_NAME} ${CMAKE_SOURCE_DIR}/memmap_custom.ld)

target_include_directories(${PROGRAM_NAME} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts
        ${CMAKE_CURRENT_LIST_DIR}/include
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
        pico_httpd_content
        )

pico_add_library(pico_httpd_content NOFLAG)
pico_set_lwip_httpd_content(pico_httpd_content INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/html_files/index.shtml
        ${CMAKE_CURRENT_LIST_DIR}/html_files/config.shtml
        ${CMAKE_CURRENT_LIST_DIR}/html_files/page2.shtml
        ${CMAKE_CURRENT_LIST_DIR}/html_files/ip_config.js
        ${CMAKE_CURRENT_LIST_DIR}/html_files/sprajax.js
        ${CMAKE_CURRENT_LIST_DIR}/html_files/favicon.ico
        )

pico_add_extra_outputs(${PROGRAM_NAME})


pico_enable_stdio_usb(${PROGRAM_NAME} TRUE)
pico_enable_stdio_uart(${PROGRAM_NAME} TRUE)
