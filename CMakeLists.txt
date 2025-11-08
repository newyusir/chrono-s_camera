cmake_minimum_required(VERSION 3.20)
project(chronos_camera LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(chronos_camera WIN32
    src/main.cpp
    src/Application.cpp
    src/CaptureSession.cpp
    src/ConfigManager.cpp
    src/HotkeyManager.cpp
    src/HotkeyUtils.cpp
    src/SettingsWindow.cpp
    src/Utility.cpp
    src/WebProcessor.cpp
    resources/app.rc
)

target_include_directories(chronos_camera PRIVATE include resources)

find_package(Microsoft.Web.WebView2 CONFIG QUIET)

if(NOT Microsoft.Web.WebView2_FOUND)
    set(WEBVIEW2_SDK_DIR "" CACHE PATH "Path to the root of the WebView2 SDK (NuGet package)")
    if(NOT WEBVIEW2_SDK_DIR)
        message(FATAL_ERROR "Microsoft.Web.WebView2 not found. Either install the SDK (so find_package works) or set WEBVIEW2_SDK_DIR to the extracted NuGet package directory.")
    endif()

    set(_webview2_include "${WEBVIEW2_SDK_DIR}/build/native/include")
    if(NOT EXISTS "${_webview2_include}/WebView2.h")
        message(FATAL_ERROR "Could not find WebView2.h under ${_webview2_include}. Check WEBVIEW2_SDK_DIR.")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_webview2_arch x64)
    else()
        set(_webview2_arch x86)
    endif()

    set(_webview2_lib "${WEBVIEW2_SDK_DIR}/build/native/lib/${_webview2_arch}/WebView2LoaderStatic.lib")
    if(NOT EXISTS "${_webview2_lib}")
        message(FATAL_ERROR "Could not find WebView2LoaderStatic.lib under ${WEBVIEW2_SDK_DIR}/build/native/lib/${_webview2_arch}.")
    endif()

    target_include_directories(chronos_camera PRIVATE "${_webview2_include}")
    target_link_libraries(chronos_camera PRIVATE "${_webview2_lib}")
else()
    target_link_libraries(chronos_camera PRIVATE Microsoft.Web.WebView2::WebView2)
endif()

target_link_libraries(chronos_camera
    PRIVATE
        windowscodecs
        shlwapi
        crypt32
        ole32
        user32
        gdi32
        shell32
)

if (MSVC)
    target_compile_definitions(chronos_camera PRIVATE UNICODE _UNICODE)
endif()

