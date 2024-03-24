setlocal enabledelayedexpansion

set GLSLC_PATH=C:/VulkanSDK/1.3.268.0/Bin/glslc.exe

C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/StandardVertex.vert -o Shaders/StandardVertex.spv
C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/StandardFragment.frag -o Shaders/StandardFragment.spv

C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/BillboardsPopulating.comp -o Shaders/BillboardsPopulating.spv
C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/ParticleVertex.vert -o Shaders/ParticleVertex.spv
C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/ParticleFragment.frag -o Shaders/ParticleFragment.spv

C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/MarchingCubesAccumulation.comp -o Shaders/MarchingCubesAccumulation.spv
C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/MarchingCubesConstruction.comp -o Shaders/MarchingCubesConstruction.spv
C:/VulkanSDK/1.3.268.0/Bin/glslc.exe Shaders/MarchingCubesPresentation.comp -o Shaders/MarchingCubesPresentation.spv

:: Simulation shaders
set SHADER_DIR=Shaders/Simulation
for %%f in (%SHADER_DIR%\*.comp) do (
    set OUT_FILE=%%~dpnf.spv
    "!GLSLC_PATH!" "%%f" -o "!OUT_FILE!"
)

endlocal
pause
