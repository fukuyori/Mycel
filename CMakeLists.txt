cmake_minimum_required(VERSION 3.16)

project(Mycel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
    set(MYCEL_QT_PACKAGE Qt6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Widgets)
    set(MYCEL_QT_PACKAGE Qt5)
endif()

add_executable(mycel
    src/main.cpp
)

target_link_libraries(mycel PRIVATE ${MYCEL_QT_PACKAGE}::Widgets)

target_compile_options(mycel PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall -Wextra -Wpedantic>
)
