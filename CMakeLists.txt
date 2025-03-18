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

# Disable several build targets for Slang to reduce build time
set(SLANG_ENABLE_SLANG_RHI OFF)
set(SLANG_ENABLE_GFX OFF)
set(SLANG_ENABLE_DX_ON_VK OFF)
set(SLANG_ENABLE_FULL_IR_VALIDATION OFF)
set(SLANG_ENABLE_IR_BREAK_ALLOC OFF)
set(SLANG_ENABLE_ASAN OFF)
set(SLANG_ENABLE_SLANGD OFF)
set(SLANG_ENABLE_SLANGC OFF)
set(SLANG_ENABLE_SLANG_GLSLANG OFF)
set(SLANG_ENABLE_TESTS OFF)
set(SLANG_ENABLE_EXAMPLES OFF)
set(SLANG_ENABLE_REPLAYER OFF)
set(SLANG_GITHUB_TOKEN OFF)
set(SLANG_ENABLE_CUDA OFF)
set(SLANG_ENABLE_OPTIX OFF)
set(SLANG_ENABLE_NVAPI OFF)
set(SLANG_ENABLE_AFTERMATH OFF)
add_subdirectory(${LIB_DIR}/slang)

add_subdirectory(${SRC_DIR}/Core)
add_subdirectory(${SRC_DIR}/Standalone)