cmake_minimum_required(VERSION 3.16)

project(Mycel VERSION 0.5.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia MultimediaWidgets CorePrivate)
find_package(Qt6Quick QUIET CONFIG)
find_package(Qt6QuickWidgets QUIET CONFIG)
find_package(Qt6PrintSupport QUIET CONFIG)
find_package(Qt6WebChannel QUIET CONFIG)
find_package(Qt6Positioning QUIET CONFIG)
find_package(Qt6WebEngineCore QUIET CONFIG)
find_package(Qt6WebEngineWidgets QUIET CONFIG)
find_package(Qt6Pdf QUIET CONFIG)

add_executable(mycel
    src/main.cpp
    assets/mycel.qrc
)

if(APPLE)
    target_sources(mycel PRIVATE assets/mycel.icns)
    set_source_files_properties(assets/mycel.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    set_target_properties(mycel PROPERTIES
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME Mycel
        MACOSX_BUNDLE_BUNDLE_NAME Mycel
        MACOSX_BUNDLE_GUI_IDENTIFIER org.spumoni.mycel
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_ICON_FILE mycel.icns
        MACOSX_BUNDLE_COPYRIGHT "Copyright (c) 2026 Noriaki Fukuyori"
    )
endif()

if(WIN32)
    target_sources(mycel PRIVATE assets/mycel.rc)
    set_target_properties(mycel PROPERTIES WIN32_EXECUTABLE ON)
endif()

target_link_libraries(mycel PRIVATE Qt6::Widgets Qt6::Network Qt6::Multimedia Qt6::MultimediaWidgets)
# QZipReader (used to extract EPUB cover images) lives in Qt Core's private API.
target_link_libraries(mycel PRIVATE Qt6::CorePrivate)

if(TARGET Qt6::WebEngineWidgets)
    target_link_libraries(mycel PRIVATE Qt6::WebEngineWidgets)
    target_compile_definitions(mycel PRIVATE MYCEL_HAS_WEBENGINE=1)
else()
    target_compile_definitions(mycel PRIVATE MYCEL_HAS_WEBENGINE=0)
endif()

if(TARGET Qt6::Pdf)
    target_link_libraries(mycel PRIVATE Qt6::Pdf)
    target_compile_definitions(mycel PRIVATE MYCEL_HAS_PDF=1)
else()
    target_compile_definitions(mycel PRIVATE MYCEL_HAS_PDF=0)
endif()

target_compile_definitions(mycel PRIVATE
    MYCEL_VERSION=\"${PROJECT_VERSION}\"
)

target_compile_options(mycel PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)

# Tests for the pure core logic (no GUI). Run with: ctest --test-dir build
enable_testing()

find_package(Qt6 QUIET COMPONENTS Core)
if(TARGET Qt6::Core)
    add_executable(mycel_fileops_test tests/fileops_test.cpp)
    set_target_properties(mycel_fileops_test PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
    target_include_directories(mycel_fileops_test PRIVATE src)
    target_link_libraries(mycel_fileops_test PRIVATE Qt6::Core)
    add_test(NAME fileops COMMAND mycel_fileops_test)
endif()
