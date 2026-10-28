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
    src/gui_icons.cpp
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

# The desktop entry names the installed binary by its full path instead of by bare name: a
# session launcher (KDE, GNOME) starts apps with a PATH that does not include ~/.local/bin, so
# `Exec=aeroclicker` failed for every per-user install. The path is substituted here, at
# install time, so it follows whichever prefix was used (cmake --install build --prefix ...).
install(CODE "
  file(READ \"${CMAKE_CURRENT_SOURCE_DIR}/aeroclicker.desktop\" _entry)
  string(REPLACE \"Exec=aeroclicker\"
                 \"Exec=\${CMAKE_INSTALL_PREFIX}/bin/aeroclicker\" _entry \"\${_entry}\")
  file(WRITE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/share/applications/aeroclicker.desktop\"
             \"\${_entry}\")
")

# The window icon is looked up by name from the icon theme (see the app id hint in src/gui.cpp),
# so the desktop entry's Icon= name needs one file per theme size. The PNGs are already baked
# from assets/aeroclicker.png and committed, so nothing has to be generated at build time.
foreach(size 16 24 32 48 64 128 256 512)
    install(FILES assets/icons/app/${size}x${size}/aeroclicker.png
            DESTINATION share/icons/hicolor/${size}x${size}/apps)
endforeach()
