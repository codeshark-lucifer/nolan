@echo off
setlocal

echo ========================================
echo        OLIA Vulkan Debug Test
echo ========================================
echo.

set "VULKAN_DIR=%~dp0vendor\vulkan"
set "VK_LAYER_PATH=%VULKAN_DIR%\bin"
set "PATH=%VULKAN_DIR%\bin;%PATH%"

echo Vulkan directory:
echo   %VULKAN_DIR%
echo.

echo Validation layer:
echo   %VK_LAYER_PATH%
echo.

echo Starting application...
echo ========================================
echo.

build\app\application.exe

echo.
echo ========================================
echo Application exited.
echo ========================================

pause