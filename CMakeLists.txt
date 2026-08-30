cmake_minimum_required(VERSION 3.16)
project(wlcrosshair VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS Gui)
find_package(LayerShellQt REQUIRED)

add_executable(wlcrosshair src/main.cpp)

target_link_libraries(wlcrosshair PRIVATE
    Qt6::Gui
    LayerShellQt::Interface
)

include(GNUInstallDirs)
install(TARGETS wlcrosshair RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
