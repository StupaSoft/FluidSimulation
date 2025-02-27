set(STANDALONE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(Standalone
    main.cpp
    MainApplication.h
    MainApplication.cpp
)

target_include_directories(Standalone PRIVATE
    ${Vulkan_INCLUDE_DIRS}

    ${SRC_DIR}/Common

    ${STANDALONE_DIR}
    ${SRC_DIR}/Core
    ${SRC_DIR}/Core/Mesh
    ${SRC_DIR}/Core/Presentation
    ${SRC_DIR}/Core/Simulation
    ${SRC_DIR}/Core/UI
    ${SRC_DIR}/Core/Utility
    
    ${LIB_DIR}/imgui
    ${LIB_DIR}/glfw/include
)

target_link_libraries(Standalone PRIVATE 
    Core
    ${LIB_DIR}/glfw/lib-vc2022/glfw3.lib
)

# Search for slang.dll
file(GLOB_RECURSE SLANG_DLL_PATH ${CMAKE_BINARY_DIR}/Libraries/slang/slang.dll)

# If slang.dll is found, copy it next to Standalone.exe
if (SLANG_DLL_PATH)
    add_custom_command(TARGET Standalone POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SLANG_DLL_PATH}"
        "$<TARGET_FILE_DIR:Standalone>"
    )
endif()
