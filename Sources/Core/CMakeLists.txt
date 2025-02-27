set(CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_library(Core STATIC
    Camera.h
    Camera.cpp
    ComputeBase.h
    ComputeBase.cpp
    DescriptorHelper.h
    DescriptorHelper.cpp
    DirectionalLight.h
    DirectionalLight.cpp
    ModelBase.h
    ModelBase.cpp
    VulkanCore.h
    VulkanCore.cpp
    VulkanResources.h
    VulkanResources.cpp
    ShaderManager.h
    ShaderManager.cpp
    ShaderResource.h
    ShaderResource.cpp

    Mesh/MeshModel.h
    Mesh/MeshModel.cpp
    Mesh/MeshObject.h
    Mesh/MeshObject.cpp
    Mesh/Triangle.h
    Mesh/Triangle.cpp    
    Mesh/Vertex.h
    Mesh/Vertex.cpp

    Presentation/Billboards.h
    Presentation/Billboards.cpp
    Presentation/BillboardsCompute.h
    Presentation/BillboardsCompute.cpp
    Presentation/MarchingCubes.h
    Presentation/MarchingCubes.cpp
    Presentation/MarchingCubesCompute.h
    Presentation/MarchingCubesCompute.cpp
    Presentation/MarchingCubesTable.cpp

    Simulation/BVH.h
    Simulation/BVH.cpp
    Simulation/CPUSimulatedScene.h
    Simulation/CPUSimulatedScene.cpp
    Simulation/GPUSimulatedScene.h
    Simulation/GPUSimulatedScene.cpp
    Simulation/HashGrid.h
    Simulation/HashGrid.cpp
    Simulation/Kernel.h
    Simulation/Kernel.cpp
    Simulation/SimulatedSceneBase.h
    Simulation/SimulatedSceneBase.cpp
    Simulation/SimulationCompute.h
    Simulation/SimulationCompute.cpp
    Simulation/SimulationParameters.h

    UI/PanelBase.h
    UI/RenderingPanel.h
    UI/RenderingPanel.cpp
    UI/SimulationPanel.h
    UI/SimulationPanel.cpp
    UI/UIModel.h
    UI/UIModel.cpp

    Utility/MathUtil.h
    Utility/MathUtil.cpp
    Utility/VulkanUtility.h
    Utility/VulkanUtility.cpp

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

target_include_directories(Core PUBLIC
    ${Vulkan_INCLUDE_DIRS}

    ${SRC_DIR}/Common

    ${CORE_DIR}
    ${CORE_DIR}/Mesh
    ${CORE_DIR}/Presentation
    ${CORE_DIR}/Simulation
    ${CORE_DIR}/UI
    ${CORE_DIR}/Utility

    ${LIB_DIR}/slang/include
    ${LIB_DIR}/imgui
    ${LIB_DIR}/glfw/include
    ${LIB_DIR}/stb_image
    ${LIB_DIR}/tinyobjloader
)

target_link_libraries(Core PRIVATE 
    slang
    Vulkan::Vulkan
)
