@echo off
setlocal enabledelayedexpansion

set GLSLC_PATH=C:/VulkanSDK/1.3.268.0/Bin/glslc.exe

:: Rendering shaders
set SHADER_DIR=Shaders\Rendering
for %%f in (%SHADER_DIR%\*.vert) do (
    set OUT_FILE=%%~dpnf.spv
    "!GLSLC_PATH!" "%%f" -o "!OUT_FILE!"

    echo Compiling Shader: %%f
)

set SHADER_DIR=Shaders\Rendering
for %%f in (%SHADER_DIR%\*.frag) do (
    set OUT_FILE=%%~dpnf.spv
    "!GLSLC_PATH!" "%%f" -o "!OUT_FILE!"

    echo Compiling Shader: %%f
)

:: Presentation shaders
set SHADER_DIR=Shaders\Presentation
for %%f in (%SHADER_DIR%\*.comp) do (
    set OUT_FILE=%%~dpnf.spv
    "!GLSLC_PATH!" "%%f" -o "!OUT_FILE!"

    echo Compiling Shader: %%f
)

:: Simulation shaders
set SHADER_DIR=Shaders\Simulation
for %%f in (%SHADER_DIR%\*.comp) do (
    set OUT_FILE=%%~dpnf.spv
    "!GLSLC_PATH!" "%%f" -o "!OUT_FILE!"

    echo Compiling Shader: %%f
)

endlocal
pause
