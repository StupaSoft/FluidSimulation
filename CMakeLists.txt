cmake_minimum_required(VERSION 3.15)
project(FluidSimulation)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Set variables
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Libraries)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Sources)

# Preprocessor definitions
add_definitions(-DSHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Shaders/")
add_definitions(-DTEXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Textures/")
add_definitions(-DMODEL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Models/")

# Build targets
set(SLANG_ENABLE_SLANG_RHI OFF)
add_subdirectory(${LIB_DIR}/slang)
add_subdirectory(${SRC_DIR}/Core)
add_subdirectory(${SRC_DIR}/Standalone)