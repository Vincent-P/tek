set common_flags=-isystem src\libs
set sdl_flags=-lSDL3
set vulkan_flags=-isystem %VULKAN_SDK%/include -L %VULKAN_SDK%/lib -lvulkan-1 -luser32

clang.exe src/main.c -o game.exe -g %common_flags% %sdl_flags% %vulkan_flags%
clang.exe src/cooker.c -o cooker.exe -g %common_flags% %vulkan_flags% -lshaderc_shared
