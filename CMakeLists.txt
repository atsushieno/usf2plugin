cmake_minimum_required(VERSION 3.18)

project(UbiquitousSF2Plugin)

set(CMAKE_CXX_STANDARD 23)

include(cmake/CPM.cmake)

add_subdirectory(external/DPF)

if (WIN32)
    set(dpfwebui_PLAT_SOURCES
            external/dpfwebui/webui/src/ui/windows/EdgeWebView.cpp
            external/dpfwebui/webui/src/ui/windows/WebView2EventHandler.cpp
            external/dpfwebui/webui/src/ui/windows/WindowsWebViewUI.cpp
    )
elseif (UNIX AND NOT APPLE)
    set(dpfwebui_PLAT_SOURCES
            external/dpfwebui/webui/src/ui/linux/CefHelper.cpp
            external/dpfwebui/webui/src/ui/linux/ChildProcessWebView.cpp
            external/dpfwebui/webui/src/ui/linux/IpcChannel.cpp
            external/dpfwebui/webui/src/ui/linux/LinuxWebViewUI.cpp
    )
endif ()

set(dpfwebui_SOURCES
        external/dpfwebui/webui/src/JSONVariant.cpp
        external/dpfwebui/webui/src/dsp/PluginEx.cpp
        external/dpfwebui/webui/src/ui/NetworkUI.cpp
        external/dpfwebui/webui/src/ui/UIEx.cpp
        #external/dpfwebui/webui/src/ui/WebServer.cpp
        external/dpfwebui/webui/src/ui/WebUIBase.cpp
        external/dpfwebui/webui/src/ui/WebViewBase.cpp
        external/dpfwebui/webui/src/ui/WebViewUI.cpp
        ${dpfwebui_PLAT_SOURCES}
)

dpf_add_plugin(UbiquitousSF2Plugin
    TARGETS clap jack lv2 vst3
    FILES_DSP
        src/PluginProcessor.cpp
        src/GeneralUser-GS.cpp
    FILES_UI
        # ${dpfwebui_SOURCES}
        src/PluginUI.cpp
    UI_TYPE webview
)

include_directories(${PROJECT_NAME}
        src
        external/DPF
        external/sf2hpp
        #external/dpfwebui/webui
        #external/dpfwebui/webui/src
        #external/dpfwebui/webui/src/ui # Why can't WebViewUI.hpp cannot resolve WebViewBase.hpp in the same directory??
)