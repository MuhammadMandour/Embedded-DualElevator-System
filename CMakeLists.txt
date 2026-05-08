cmake_minimum_required(VERSION 3.30)
include(cmake/ArmToolchain.cmake)

project(stm32-sec6 C)

set(CMAKE_C_STANDARD                11)
set(CMAKE_C_STANDARD_REQUIRED       ON)
set(CMAKE_C_EXTENSIONS              ON)
enable_language(ASM)

set(PROJECT_PATH                         ${CMAKE_CURRENT_SOURCE_DIR})
include(cmake/Device.cmake)
include(cmake/SourceList.cmake)
include(cmake/IncludeList.cmake)

set(LINKER_SCRIPTS_BASE_DIR              ${PROJECT_PATH}/STM32-base/linker/)
set(STARTUP_BASE_DIR                     ${PROJECT_PATH}/STM32-base/startup/)

# Separate main files from common source files
set(COMMON_SOURCES ${SOURCE_LIST})
list(FILTER COMMON_SOURCES EXCLUDE REGEX "Master/main.c|Slave/main.c")

# Define a macro to create firmware targets
macro(add_firmware_target TARGET_NAME MAIN_FILE)
    # Executable files
    add_executable(${TARGET_NAME} ${COMMON_SOURCES} ${MAIN_FILE} ${STARTUP_BASE_DIR}/${SERIES_FOLDER}/${MAPPED_DEVICE}.s
            Usart/Usart.h
            Spi/Spi.c
            Rcc/Rcc.c)

    # Include
    target_include_directories(${TARGET_NAME} PUBLIC ${INCLUDE_LIST})
    # Project symbols
    target_compile_definitions(${TARGET_NAME} PUBLIC ${COMPILER_DEFINE})
    # Compiler options
    target_compile_options(${TARGET_NAME} PUBLIC
            ${CPU_PARAMETERS}
            -Wall
            -Wpedantic
            -Wno-unused-parameter
    )

    # Linker options
    target_link_options(${TARGET_NAME} PRIVATE
            ${CPU_PARAMETERS}
            -L${LINKER_SCRIPTS_BASE_DIR}
            -T${LINKER_SCRIPTS_BASE_DIR}/${SERIES_FOLDER}/${DEVICE}.ld
            -Xlinker --gc-sections
            -Wl,-Map=${TARGET_NAME}.map
            --specs=nano.specs
    )

    # Execute post-build to print size
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${TARGET_NAME}>
    )
    # Convert output to hex and binary
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.hex
    )

    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${TARGET_NAME}> ${TARGET_NAME}.bin
    )
endmacro()

# Build Master firmware
add_firmware_target(stm32-sec6-master Master/main.c)

# Build Slave firmware
add_firmware_target(stm32-sec6-slave Slave/main.c)
