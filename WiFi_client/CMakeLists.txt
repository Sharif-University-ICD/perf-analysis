# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "/home/mohsen/.pico-sdk/sdk/2.1.0")

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(WiFi_client C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(WiFi_client WiFi_client.c )

add_subdirectory(../aes aes)
include_directories(../aes)

pico_set_program_name(WiFi_client "WiFi_client")
pico_set_program_version(WiFi_client "0.1")

pico_enable_stdio_uart(WiFi_client 1)
pico_enable_stdio_usb(WiFi_client 1)

# Add the standard library to the build
target_link_libraries(WiFi_client
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(WiFi_client PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(WiFi_client 
        pico_cyw43_arch_lwip_poll
	tiny-aes
        )

pico_add_extra_outputs(WiFi_client)

