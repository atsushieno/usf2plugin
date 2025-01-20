cmake_minimum_required(VERSION 3.18)

project(USF2Plugin)

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

dpf_add_plugin(USF2Plugin
    TARGETS clap lv2 vst3 au
    FILES_DSP
        src/Usf2Plugin.cpp
        src/GeneralUser-GS.cpp
        src/SF2Application.cpp
    FILES_UI
        src/PluginUI.cpp
    UI_TYPE webview
    USE_WEB_VIEW true
)

include_directories(
        src
        external/DPF
        ${tinysoundfont_SOURCE_DIR}
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
)

foreach (USF2_DST_DIR "vst3" "component" "lv2" "clap")
if(APPLE)
    file(INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/src/index.html"
            DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.${USF2_DST_DIR}/Contents/Resources/"
    )
else()
    # FIXME: verify on all platforms
    file(INSTALL "${CMAKE_CURRENT_SOURCE_DIR}/src/index.html"
            DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.${USF2_DST_DIR}"
    )
endif()
endforeach()
