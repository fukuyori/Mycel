cmake_minimum_required(VERSION 3.16)

project(Mycel VERSION 0.3.2 LANGUAGES CXX)

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
