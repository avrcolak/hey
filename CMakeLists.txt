cmake_minimum_required(VERSION 3.15)

cmake_policy(SET CMP0091 NEW)

project(hey)

add_executable(vectorwar 
    main.cpp 
    vectorwar.cpp 
    gamestate.cpp 
    sdl_renderer.cpp 
    windows_shims.cpp
    imgui-8bcac7d9/imgui.cpp
    imgui-8bcac7d9/imgui_widgets.cpp
    imgui-8bcac7d9/imgui_draw.cpp
    imgui-8bcac7d9/imgui_demo.cpp
    imgui-8bcac7d9/imgui_impl_opengl2.cpp
    imgui-8bcac7d9/imgui_impl_sdl.cpp)

set_property(TARGET vectorwar PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
	set_property(TARGET vectorwar PROPERTY
        LINK_FLAGS "/NODEFAULTLIB:MSVCRT /NODEFAULTLIB:MSVCPRT")
endif()

target_include_directories(vectorwar PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.10/include
  ${CMAKE_CURRENT_LIST_DIR}/ggpo-4b52427/include
  ${CMAKE_CURRENT_LIST_DIR}/imgui-8bcac7d9)

target_link_directories(vectorwar PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/SDL2-2.0.10/lib
  ${CMAKE_CURRENT_LIST_DIR}/ggpo-4b52427/lib)

target_link_libraries(vectorwar 
    SDL2-static debug SDL2-staticd
    SDL2main debug SDL2maind
    GGPO debug GGPOd
    winmm
    imm32
    version
    Setupapi
    ws2_32
    opengl32)