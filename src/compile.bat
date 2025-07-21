:: set common_flags=-isystem src\libs
:: set sdl_flags=-lSDL3
:: set vulkan_flags=-isystem %VULKAN_SDK%/include -L %VULKAN_SDK%/lib -lvulkan-1 -luser32
:: set imgui_flags=-L src\libs\imgui_build -limgui
:: set tracy_flags=-ltracy -DTRACY_ENABLE
:: set ggpo_flags=-lggpo -lWinmm
:: clang.exe src/main.c -o game.exe -g %common_flags% %sdl_flags% %vulkan_flags% %imgui_flags% %tracy_flags% %ggpo_flags%
:: clang.exe src/cooker.c -o cooker.exe -g %common_flags% %vulkan_flags% -lshaderc_shared


call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" x64
set common_flags=/std:clatest /external:W0 /external:I src\libs /Zi
set sdl_link_flags=SDL3.lib
set vulkan_flags=/external:I %VULKAN_SDK%/include
set vulkan_link_flags=/LIBPATH:%VULKAN_SDK%/lib vulkan-1.lib user32.lib
set imgui_link_flags=/LIBPATH:src\libs\imgui_build imgui.lib
set tracy_flags=/D TRACY_ENABLE
set tracy_link_flags=tracy.lib
set ggpo_link_flags=ggpo.lib Winmm.lib ws2_32.lib
cl.exe src/main.c %common_flags% %vulkan_flags% /link /out:game.exe %sdl_link_flags% %vulkan_link_flags% %imgui_link_flags% %tracy_link_flags% %ggpo_link_flags% /DEBUG:FULL
