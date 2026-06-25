cmake_minimum_required(VERSION 3.16)

project(Mycel VERSION 0.3.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Multimedia MultimediaWidgets)

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

target_link_libraries(mycel PRIVATE Qt6::Widgets Qt6::Multimedia Qt6::MultimediaWidgets)

target_compile_definitions(mycel PRIVATE
    MYCEL_VERSION=\"${PROJECT_VERSION}\"
)

target_compile_options(mycel PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)
