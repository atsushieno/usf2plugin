cmake_minimum_required(VERSION 3.18)

project(UbiquitousSF2Plugin)

set(CMAKE_CXX_STANDARD 20)

include(cmake/CPM.cmake)

add_subdirectory(external/DPF)

#### Externals

include(FetchContent)

FetchContent_Declare(
        tinysoundfont
        GIT_REPOSITORY https://github.com/atsushieno/TinySoundFont
        GIT_TAG        994c5c86e6e61cd8f0fced0dc628dbedcef7ca6d
)
FetchContent_MakeAvailable(tinysoundfont)

#### DPF

dpf_add_plugin(UbiquitousSF2Plugin
    TARGETS clap jack lv2 vst3
    FILES_DSP
        src/PluginProcessor.cpp
        src/GeneralUser-GS.cpp
        src/SF2Application.cpp
    FILES_UI
        src/PluginUI.cpp
    UI_TYPE webview
)

include_directories(
        src
        external/DPF
        ${tinysoundfont_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
)
