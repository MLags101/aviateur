cmake_minimum_required(VERSION 3.18)

# This should go before project declaration.
if (WIN32)
    message("[Aviateur] Using vcpkg in <$ENV{VCPKG_ROOT}>, which should be the vcpkg dir in which you have installed the dependency libs.")
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
endif ()

project(aviateur
        VERSION 0.1
        LANGUAGES C CXX)

if (APPLE)
    enable_language(OBJC OBJCXX)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3" CACHE STRING "Minimum macOS version")
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Target architectures")
endif ()

set(CMAKE_CXX_STANDARD 20)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(AVIATEUR_ENABLE_OPENCV "Enable OpenCV" OFF)

find_package(PkgConfig REQUIRED)

if (WIN32)
    pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

    find_package(FFMPEG REQUIRED)
    find_package(unofficial-sodium CONFIG REQUIRED)
else ()
    pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
            libavformat
            libavcodec
            libswresample
            libswscale
            libavutil
    )

    pkg_check_modules(LIBSODIUM REQUIRED IMPORTED_TARGET libsodium)
endif ()

if (AVIATEUR_ENABLE_OPENCV)
    find_package(OpenCV REQUIRED)
endif ()

if (APPLE)
    add_executable(${PROJECT_NAME} MACOSX_BUNDLE
            src/main.cpp
            src/gui_interface.h
    )

    # Copy assets.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets

            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/3rd/vecgui/assets
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets

            # Sync to App Bundle
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
            ${CMAKE_BINARY_DIR}/bin/aviateur.app/Contents/Resources/assets
    )
else ()
    add_executable(${PROJECT_NAME}
            src/main.cpp
            src/gui_interface.h
    )

    # Copy assets.
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets

            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/3rd/vecgui/assets
            ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    )
endif ()

if (AVIATEUR_ENABLE_OPENCV)
    target_compile_definitions(
            ${PROJECT_NAME} PRIVATE
            AVIATEUR_USE_OPENCV
    )
endif ()

# Enable SIMD FEC
target_compile_definitions(${PROJECT_NAME} PRIVATE
        ZFEX_UNROLL_ADDMUL_SIMD=8
        ZFEX_USE_INTEL_SSSE3
        ZFEX_USE_ARM_NEON
        ZFEX_INLINE_ADDMUL
        ZFEX_INLINE_ADDMUL_SIMD
)

add_subdirectory(src/gui)
add_subdirectory(src/player)
add_subdirectory(src/wifi)
add_subdirectory(src/feature)

if (MSVC)
    string(APPEND CMAKE_CXX_FLAGS " /utf-8")
endif ()

if (NOT EXISTS "${CMAKE_SOURCE_DIR}/3rd/devourer/src/RtlJaguarDevice.h")
    message(FATAL_ERROR
            "The devourer submodule is not at Aviateur's required AWUS1900 revision. "
            "Run: git submodule update --init --recursive && "
            "git -C 3rd/devourer checkout e74d57e96b8e47a6ea6dae6778414e1bf09ddbc2")
endif ()

add_subdirectory(3rd/devourer)
target_include_directories(${PROJECT_NAME} PRIVATE "3rd/devourer/src" "3rd/devourer/hal")
if (WIN32 AND MINGW)
    # devourer contains both <libusb.h> and <libusb-1.0/libusb.h> includes.
    # PkgConfig::LIBUSB supplies the nested directory; its parent is required
    # for the latter form used by MinGW code paths and demo targets.
    target_include_directories(WiFiDriver PUBLIC "${LIBUSB_INCLUDEDIR}")

    # Aviateur's Windows integration is receive-only. These optional devourer
    # transmit utilities include POSIX-only process control and are not part of
    # the application or WiFiDriverDemo acceptance path.
    set_target_properties(
            WiFiDriverTxDemo
            PrecoderDemo
            StreamTxDemo
            SvcTxDemo
            StreamDuplexDemo
            PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif ()

# A Linux-hosted MinGW build must not consume the host Vulkan SDK. Pathfinder's
# OpenGL backend is supported on Windows and does not require target Vulkan
# headers or a Windows Vulkan loader.
if (WIN32 AND MINGW)
    set(VECGUI_VULKAN OFF CACHE BOOL "Use Vulkan instead of OpenGL" FORCE)
endif ()

add_subdirectory(3rd/vecgui)
target_include_directories(${PROJECT_NAME} PRIVATE "3rd/vecgui/src")
if (WIN32 AND MINGW AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    # Pathfinder enables SSE4.1 intrinsics for non-ARM Windows targets but its
    # bundled CMake only adds the matching compiler option on Linux.
    target_compile_options(pathfinder PUBLIC -msse4.1)
endif ()

add_subdirectory(3rd/json)
target_include_directories(${PROJECT_NAME} PRIVATE "3rd/json/include")

add_subdirectory(3rd/mINI)
target_include_directories(${PROJECT_NAME} PRIVATE "3rd/mINI/src")

add_subdirectory(3rd/SDL)
target_include_directories(${PROJECT_NAME} PRIVATE "3rd/SDL/include")

target_include_directories(${PROJECT_NAME} PRIVATE "src/wifi/wfb-ng/include")

if (WIN32)
    target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
    target_link_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE
            ${FFMPEG_LIBRARIES}
            PkgConfig::LIBUSB
            unofficial-sodium::sodium
            WiFiDriver
            vecgui
            SDL3::SDL3-static
            ws2_32
    )
else ()
    target_link_libraries(${PROJECT_NAME} PRIVATE
            PkgConfig::LIBAV
            PkgConfig::LIBSODIUM
            WiFiDriver
            vecgui
            SDL3::SDL3-static
            pcap
    )
endif ()

if (AVIATEUR_ENABLE_OPENCV)
    target_link_libraries(${PROJECT_NAME} PRIVATE
            ${OpenCV_LIBS}
    )
endif ()

# Deployment
if (APPLE)
    set(MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/assets/Info.plist.in")
    set(MACOSX_BUNDLE_GUI_IDENTIFIER "com.aviateur.app")
    set(MACOSX_BUNDLE_BUNDLE_NAME "Aviateur")
    set(MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}")
    set(MACOSX_BUNDLE_ICON_FILE "logo.icns")

    set_target_properties(${PROJECT_NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${MACOSX_BUNDLE_INFO_PLIST}"
            MACOSX_BUNDLE_GUI_IDENTIFIER "${MACOSX_BUNDLE_GUI_IDENTIFIER}"
            MACOSX_BUNDLE_BUNDLE_NAME "${MACOSX_BUNDLE_BUNDLE_NAME}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${MACOSX_BUNDLE_BUNDLE_VERSION}"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${MACOSX_BUNDLE_SHORT_VERSION_STRING}"
            MACOSX_BUNDLE_ICON_FILE "${MACOSX_BUNDLE_ICON_FILE}"
    )

    install(TARGETS ${PROJECT_NAME}
            BUNDLE DESTINATION .
            COMPONENT Runtime
    )

    install(FILES "${CMAKE_SOURCE_DIR}/assets/logo.icns"
            DESTINATION "aviateur.app/Contents/Resources"
            COMPONENT Runtime
    )

    include(InstallRequiredSystemLibraries)
    include(CPack)

    set(CPACK_PACKAGE_NAME "Aviateur")
    set(CPACK_PACKAGE_VENDOR "Aviateur")
    set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Aviateur - OpenIPC FPV Ground Station")
    set(CPACK_PACKAGE_FILE_NAME "Aviateur-${PROJECT_VERSION}-macOS")
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "Aviateur")
    set(CPACK_DMG_FORMAT "UDBZ")
    set(CPACK_DMG_DS_STORE_SETUP_SCRIPT "${CMAKE_SOURCE_DIR}/assets/dmg-setup.scpt")
endif ()
