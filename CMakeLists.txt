# Basic setup
cmake_minimum_required(VERSION 3.15)
project(FluidSimulation)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Set variables
set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Libraries)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Sources)
set(COMMON_DIR ${SRC_DIR}/Common)

# Build targets
#add_subdirectory(${LIB_DIR}/slang)

# Include directories
include_directories(${COMMON_DIR})
include_directories(${Vulkan_INCLUDE_DIRS}) # Vulkan headers

# ========== Core ==========
set(CORE_DIR ${SRC_DIR}/Core)

# Will be used for looking up resources
add_definitions(-DSHADER_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Shaders/")
add_definitions(-DTEXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Textures/")
add_definitions(-DMODEL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/Models/")

add_library(Core STATIC
    ${CORE_DIR}/Camera.h
    ${CORE_DIR}/Camera.cpp
    ${CORE_DIR}/ComputeBase.h
    ${CORE_DIR}/ComputeBase.cpp
    ${CORE_DIR}/DescriptorHelper.h
    ${CORE_DIR}/DescriptorHelper.cpp
    ${CORE_DIR}/DirectionalLight.h
    ${CORE_DIR}/DirectionalLight.cpp
    ${CORE_DIR}/ModelBase.h
    ${CORE_DIR}/ModelBase.cpp
    ${CORE_DIR}/VulkanCore.h
    ${CORE_DIR}/VulkanCore.cpp
    ${CORE_DIR}/VulkanResources.h
    ${CORE_DIR}/VulkanResources.cpp
    ${CORE_DIR}/ShaderManager.h
    ${CORE_DIR}/ShaderManager.cpp

    ${CORE_DIR}/Mesh/MeshModel.h
    ${CORE_DIR}/Mesh/MeshModel.cpp
    ${CORE_DIR}/Mesh/MeshObject.h
    ${CORE_DIR}/Mesh/MeshObject.cpp
    ${CORE_DIR}/Mesh/Triangle.h
    ${CORE_DIR}/Mesh/Triangle.cpp    
    ${CORE_DIR}/Mesh/Vertex.h
    ${CORE_DIR}/Mesh/Vertex.cpp

    ${CORE_DIR}/Presentation/Billboards.h
    ${CORE_DIR}/Presentation/Billboards.cpp
    ${CORE_DIR}/Presentation/BillboardsCompute.h
    ${CORE_DIR}/Presentation/BillboardsCompute.cpp
    ${CORE_DIR}/Presentation/MarchingCubes.h
    ${CORE_DIR}/Presentation/MarchingCubes.cpp
    ${CORE_DIR}/Presentation/MarchingCubesCompute.h
    ${CORE_DIR}/Presentation/MarchingCubesCompute.cpp
    ${CORE_DIR}/Presentation/MarchingCubesTable.cpp

    ${CORE_DIR}/Simulation/BVH.h
    ${CORE_DIR}/Simulation/BVH.cpp
    ${CORE_DIR}/Simulation/CPUSimulatedScene.h
    ${CORE_DIR}/Simulation/CPUSimulatedScene.cpp
    ${CORE_DIR}/Simulation/GPUSimulatedScene.h
    ${CORE_DIR}/Simulation/GPUSimulatedScene.cpp
    ${CORE_DIR}/Simulation/HashGrid.h
    ${CORE_DIR}/Simulation/HashGrid.cpp
    ${CORE_DIR}/Simulation/Kernel.h
    ${CORE_DIR}/Simulation/Kernel.cpp
    ${CORE_DIR}/Simulation/SimulatedSceneBase.h
    ${CORE_DIR}/Simulation/SimulatedSceneBase.cpp
    ${CORE_DIR}/Simulation/SimulationCompute.h
    ${CORE_DIR}/Simulation/SimulationCompute.cpp
    ${CORE_DIR}/Simulation/SimulationParameters.h

    ${CORE_DIR}/UI/PanelBase.h
    ${CORE_DIR}/UI/RenderingPanel.h
    ${CORE_DIR}/UI/RenderingPanel.cpp
    ${CORE_DIR}/UI/SimulationPanel.h
    ${CORE_DIR}/UI/SimulationPanel.cpp
    ${CORE_DIR}/UI/UIModel.h
    ${CORE_DIR}/UI/UIModel.cpp

    ${CORE_DIR}/Utility/MathUtil.h
    ${CORE_DIR}/Utility/MathUtil.cpp
    ${CORE_DIR}/Utility/VulkanUtility.h
    ${CORE_DIR}/Utility/VulkanUtility.cpp

    # imgui
    ${LIB_DIR}/imgui/imgui.h
    ${LIB_DIR}/imgui/imgui.cpp
    ${LIB_DIR}/imgui/imgui_demo.cpp
    ${LIB_DIR}/imgui/imgui_draw.cpp
    ${LIB_DIR}/imgui/imgui_widgets.cpp
    ${LIB_DIR}/imgui/imgui_tables.cpp
    ${LIB_DIR}/imgui/backends/imgui_impl_vulkan.cpp
    ${LIB_DIR}/imgui/backends/imgui_impl_glfw.cpp
)

target_include_directories(Core PRIVATE
    ${CORE_DIR}
    ${CORE_DIR}/Mesh
    ${CORE_DIR}/Presentation
    ${CORE_DIR}/Simulation
    ${CORE_DIR}/UI
    ${CORE_DIR}/Utility

    ${LIB_DIR}/imgui
    ${LIB_DIR}/glfw/include
    ${LIB_DIR}/stb_image
    ${LIB_DIR}/tinyobjloader
)

target_link_libraries(Core PRIVATE 
    #slang 
    Vulkan::Vulkan
)

# ========== Standalone ==========
set(STANDALONE_DIR ${SRC_DIR}/Standalone)

add_executable(Standalone
    ${STANDALONE_DIR}/main.cpp
    ${STANDALONE_DIR}/MainApplication.h
    ${STANDALONE_DIR}/MainApplication.cpp
)

target_include_directories(Standalone PRIVATE
    ${CORE_DIR}
    ${CORE_DIR}/Mesh
    ${CORE_DIR}/Presentation
    ${CORE_DIR}/Simulation
    ${CORE_DIR}/UI
    ${CORE_DIR}/Utility

    ${LIB_DIR}/imgui
    ${LIB_DIR}/glfw/include
)

target_link_libraries(Standalone PRIVATE 
    Core
    ${LIB_DIR}/glfw/lib-vc2022/glfw3.lib
)
