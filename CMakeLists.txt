cmake_minimum_required(VERSION 3.16)
project(AeroClicker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ImGui sources
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/imgui-1.93.0)
set(IMGUI_BACKEND_DIR ${IMGUI_DIR}/backends)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_BACKEND_DIR}/imgui_impl_opengl3.cpp
)

find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(aeroclicker
    src/main.cpp
    src/gui.cpp
    src/gui_theme.cpp
    src/gui_widgets.cpp
    ${IMGUI_SOURCES}
)

target_include_directories(aeroclicker PRIVATE
    include
    ${IMGUI_DIR}
    ${IMGUI_BACKEND_DIR}
)

target_link_libraries(aeroclicker PRIVATE
    pthread
    glfw
    OpenGL::GL
)

install(TARGETS aeroclicker DESTINATION bin)
