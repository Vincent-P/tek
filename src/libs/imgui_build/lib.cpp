
// dear imgui: single-file wrapper include
// We use this to validate compiling all *.cpp files in a same compilation unit.
// Users of that technique (also called "Unity builds") can generally provide this themselves,
// so we don't really recommend you use this in your projects.

// Do this:
//    #define IMGUI_IMPLEMENTATION
// Before you include this file in *one* C++ file to create the implementation.
// Using this in your project will leak the contents of imgui_internal.h and ImVec2 operators in this compilation unit.

#define IMGUI_DEFINE_MATH_OPERATORS

#include "./imgui.h"
#ifdef IMGUI_ENABLE_FREETYPE
#include "./misc/freetype/imgui_freetype.h"
#endif

#include "./imgui.cpp"
#include "./imgui_demo.cpp"
#include "./imgui_draw.cpp"
#include "./imgui_tables.cpp"
#include "./imgui_widgets.cpp"
#ifdef IMGUI_ENABLE_FREETYPE
#include "./misc/freetype/imgui_freetype.cpp"
#endif

#include "./dcimgui.cpp"
