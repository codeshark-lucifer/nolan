:: Create the target directories
mkdir "D:\Program Education\c++\olia-engine\vendor\vulkan\include"
mkdir "D:\Program Education\c++\olia-engine\vendor\vulkan\lib"
mkdir "D:\Program Education\c++\olia-engine\vendor\vulkan\bin"

:: Copy Headers (vulkan.h, vulkan.hpp, etc.)
xcopy /E /I /Y "D:\VulkanSDK\1.4.341.1\Include" "D:\Program Education\c++\olia-engine\vendor\vulkan\include"

:: Copy Static Libraries (vulkan-1.lib, shaderc, etc.)
xcopy /E /I /Y "D:\VulkanSDK\1.4.341.1\Lib" "D:\Program Education\c++\olia-engine\vendor\vulkan\lib"

:: Copy Shader Compilers (glslangValidator, dxc) - Essential for rendering
xcopy /Y "D:\VulkanSDK\1.4.341.1\Bin\glslangValidator.exe" "D:\Program Education\c++\olia-engine\vendor\vulkan\bin\"
xcopy /Y "D:\VulkanSDK\1.4.341.1\Bin\dxc.exe" "D:\Program Education\c++\olia-engine\vendor\vulkan\bin\"