cmake_minimum_required(VERSION 3.17)

#set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
# specify cross compilers and tools
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
set(CMAKE_OBJDUMP arm-none-eabi-objdump)
set(SIZE arm-none-eabi-size)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# project settings
project(stellaris C CXX ASM)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

set(MCPU cortex-m3)

#add_compile_definitions(GCC_ARMCM3_LM3S102)
add_compile_options(-mcpu=${MCPU} -mthumb)
add_compile_options(-ffunction-sections -fdata-sections)

add_compile_definitions(PACK_STRUCT_END=__attribute\(\(packed\)\))
add_compile_definitions(ALIGN_STRUCT_END=__attribute\(\(aligned\(4\)\)\))
add_compile_definitions(sprintf=usprintf)
add_compile_definitions(snprintf=usnprintf)
add_compile_definitions(printf=uipprintf)



if ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    message(STATUS "Maximum optimization for speed")
    add_compile_options(-Ofast)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    message(STATUS "Maximum optimization for speed, debug info included")
    add_compile_options(-Ofast -g)
elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    message(STATUS "Maximum optimization for size")
    add_compile_options(-Os)
else ()
    message(STATUS "Minimal optimization, debug info included")
    add_compile_options(-Og -g)
endif ()

include_directories(
        src
        third_party/FreeRtos/include
        third_party/FreeRtos/portable/GCC/ARM_CM3
        third_party/uip
        ${CMAKE_SOURCE_DIR}/third_party/LuminaryMicro
        # here goes tcp
        third_party/FreeRtos-Plus-TCP/portable/Compiler/GCC
        third_party/FreeRtos-Plus-TCP/include

        #        ${CMAKE_SOURCE_DIR}/../Drivers/CMSIS/Include
#        #[[ TODO rest of include folder locations ex.:
#        ${CMAKE_SOURCE_DIR}/../Drivers/STM32H7xx_HAL_Driver/Inc
#        ${CMAKE_SOURCE_DIR}/../Drivers/STM32H7xx_HAL_Driver/Inc/Legacy
#        ${CMAKE_SOURCE_DIR}/../Drivers/CMSIS/Device/ST/STM32H7xx/Include
#        ]]
)



file(GLOB_RECURSE SOURCES
        "third_party/LuminaryMicro/*.c"
        third_party/FreeRtos/portable/GCC/ARM_CM3/port.c
        third_party/FreeRtos/portable/MemMang/heap_4.c
        third_party/FreeRtos/list.c
        third_party/FreeRtos/queue.c
        third_party/FreeRtos/tasks.c
        third_party/FreeRtos/event_groups.c
#        third_party/uip/uip_arp.c
#        third_party/uip/psock.c
#        third_party/uip/timer.c
#        third_party/uip/uip.c
#        "src/*.c"
        src/main.c
        src/startup.c
        src/rit128x96x4.c
        src/ethernet.c

        # here goes tcp
        third_party/FreeRtos-Plus-TCP/portable/BufferManagement/BufferAllocation_2.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_IP.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_ARP.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_DHCP.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_DNS.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_Sockets.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_TCP_IP.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_UDP_IP.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_TCP_WIN.c
        third_party/FreeRtos-Plus-TCP/FreeRTOS_Stream_Buffer.c

        )

message(ERROR "sources: ${SOURCES}")

set(LINKER_SCRIPT ${CMAKE_SOURCE_DIR}/third_party/standalone.ld)

add_link_options(-nostartfiles -Xlinker -Xlinker -M -Xlinker -Xlinker)
add_link_options(-mcpu=${MCPU} -mthumb)
add_link_options(-T ${LINKER_SCRIPT})

add_executable(${PROJECT_NAME}.elf ${SOURCES} ${LINKER_SCRIPT})
target_link_libraries(${PROJECT_NAME}.elf ${CMAKE_SOURCE_DIR}/third_party/LuminaryMicro/arm-none-eabi-gcc/libdriver.a)

#set(HEX_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.hex)
#set(BIN_FILE ${PROJECT_BINARY_DIR}/${PROJECT_NAME}.bin)
#
#add_custom_command(TARGET ${PROJECT_NAME}.elf POST_BUILD
#        COMMAND ${CMAKE_OBJCOPY} -Oihex $<TARGET_FILE:${PROJECT_NAME}.elf> ${HEX_FILE}
#        COMMAND ${CMAKE_OBJCOPY} -Obinary $<TARGET_FILE:${PROJECT_NAME}.elf> ${BIN_FILE}
#        COMMENT "Building ${HEX_FILE}
#Building ${BIN_FILE}")