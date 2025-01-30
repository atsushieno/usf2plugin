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
FetchContent_Declare(
        choc
        GIT_REPOSITORY https://github.com/Tracktion/choc/
        GIT_TAG 6dfac9fec70eae9159e64dc55538d40a2171175e
)
FetchContent_MakeAvailable(choc)
FetchContent_Declare(
        webaudio-controls
        GIT_REPOSITORY https://github.com/g200kg/webaudio-controls
        GIT_TAG c4dc6d2b31c196867e57853e7281eb7303d4f6ba
)
FetchContent_MakeAvailable(webaudio-controls)
FetchContent_Declare(
        generaluser-gs
        GIT_REPOSITORY https://github.com/mrbumpy409/GeneralUser-GS
        GIT_TAG d0fc360abafa736f11a1fa18c721f65bfc3a6991
)
FetchContent_MakeAvailable(generaluser-gs)

#### DPF

dpf_add_plugin(USF2Plugin
    TARGETS clap lv2 vst3 au
    FILES_DSP
        src/Usf2Plugin.cpp
        #src/GeneralUser-GS.cpp
        src/SF2Application.cpp
        src/Usf2EditControllerWebServer.cpp
    FILES_UI
        src/PluginUI.cpp
    UI_TYPE webview
    USE_WEB_VIEW true
)

include_directories(
        src
        external/DPF
        ${tinysoundfont_SOURCE_DIR}
        ${cpp-httplib_SOURCE_DIR}
        ${choc_SOURCE_DIR}
)

set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL CONFIG REQUIRED)

include_directories(
        ${OPENSSL_INCLUDE_DIR}
)

target_link_libraries(USF2Plugin
        PUBLIC
        ${OPENSSL_LIBRARIES}
)

# Prepare web content files
add_custom_target(build-web-content
        npm install
        WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/src/web
)
add_dependencies(${PROJECT_NAME} build-web-content)

foreach (USF2_DST_DIR "vst3" "component" "lv2" "clap")
    if(APPLE)
        set(USF2_RES_DIR_SUB "/Contents/Resources")
    endif()
    file(MAKE_DIRECTORY
            ${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.${USF2_DST_DIR}${USF2_RES_DIR_SUB}
    )
    file(CREATE_LINK
            ${CMAKE_CURRENT_SOURCE_DIR}/src/web
            ${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.${USF2_DST_DIR}${USF2_RES_DIR_SUB}/web
            COPY_ON_ERROR
            SYMBOLIC
    )
    file(INSTALL
            ${CMAKE_CURRENT_SOURCE_DIR}/src/index.html
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.${USF2_DST_DIR}${USF2_RES_DIR_SUB}
    )
    file(INSTALL
            ${webaudio-controls_SOURCE_DIR}/webaudio-controls.js
            ${webaudio-controls_SOURCE_DIR}/webcomponents-lite.js
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.${USF2_DST_DIR}${USF2_RES_DIR_SUB}/web
    )
    file(INSTALL
            ${generaluser-gs_SOURCE_DIR}/GeneralUser-GS.sf2
            DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bin/${PROJECT_NAME}.${USF2_DST_DIR}${USF2_RES_DIR_SUB}/
    )
endforeach()
