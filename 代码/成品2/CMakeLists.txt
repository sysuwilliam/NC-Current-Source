cmake_minimum_required(VERSION 3.22)

# Setup compiler settings
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# Define the build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

# Set the project name
set(CMAKE_PROJECT_NAME test)

# Include toolchain file
include("cmake/gcc-arm-none-eabi.cmake")

# Enable compile command to ease indexing with e.g. clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Core project settings
project(${CMAKE_PROJECT_NAME} C ASM)   # 合并了 enable_language
message("Build type: " ${CMAKE_BUILD_TYPE})

# Create an executable object type
add_executable(${CMAKE_PROJECT_NAME}
        Core/Lib/dsp_DAC.c
        Core/Lib/dsp_ADC.c
        Core/Lib/button.c
        Core/Lib/dsp_test_communication.c
        Core/Lib/TJC_HMI.c
        Core/Middleware/PID.c
        Core/Middleware/key_service.c
        Core/Application/global_value.c
)

# Add STM32CubeMX generated sources
add_subdirectory(cmake/stm32cubemx)

# Add include paths
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
        Core/Lib
        Core/Middleware
        Core/Application
)

# Add linked libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
        stm32cubemx
)
