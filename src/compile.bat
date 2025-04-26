set common_flags=-isystem src\libs
set sdl_flags=-lSDL3
set vulkan_flags=-isystem %VULKAN_SDK%/include -L %VULKAN_SDK%/lib -lvulkan-1 -luser32
set imgui_flags=-L src\libs\imgui_build -limgui
set tracy_flags=-ltracy -DTRACY_ENABLE

clang.exe src/main.c -o game.exe -g %common_flags% %sdl_flags% %vulkan_flags% %imgui_flags% %tracy_flags%
clang.exe src/cooker.c -o cooker.exe -g %common_flags% %vulkan_flags% -lshaderc_shared
