## NovatOS Desktop — CMakeLists.txt
## Custom lightweight desktop environment built with Qt6

cmake_minimum_required(VERSION 3.16)
project(novatos-desktop VERSION 2026.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Qt6 modules
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WaylandClient WaylandCompositor)
qt_standard_project_setup()

# ─── NovatOS Desktop Shell (main compositor + UI) ───
add_executable(novatos-desktop
    src/main.cpp
    src/DesktopShell.cpp
    src/Taskbar.cpp
    src/StartMenu.cpp
    src/LockScreen.cpp
    src/WindowManager.cpp
    src/Resources.qrc
)

target_link_libraries(novatos-desktop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WaylandClient
    Qt6::WaylandCompositor
)

target_compile_definitions(novatos-desktop PRIVATE
    QT_NO_KEYWORDS
    QT_DEPRECATED_WARNINGS
)

# Install to /usr/local/bin
install(TARGETS novatos-desktop RUNTIME DESTINATION /usr/local/bin)

# Install desktop session file
install(FILES
    assets/novatos-desktop-wayland.desktop
    DESTINATION /usr/share/wayland-sessions
)

install(FILES
    assets/novatos-desktop-x11.desktop
    DESTINATION /usr/share/xsessions
)

# Install default config
install(DIRECTORY
    assets/config/
    DESTINATION /etc/skel/.config/novatos
)
