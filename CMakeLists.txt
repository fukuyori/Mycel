cmake_minimum_required(VERSION 3.16)

project(Mycel VERSION 0.8.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# CorePrivate ヘッダの使用に伴う Qt 純正の警告を抑制（内部ヘッダ利用は意図的）
set(QT_NO_PRIVATE_MODULE_WARNING ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Multimedia MultimediaWidgets CorePrivate)

# Optional modules are searched only inside the Qt installation Qt6 itself came from
# (Qt6_DIR/.. is the cmake/ directory holding every Qt6* module of that installation).
# With more than one Qt on the machine (e.g. the official installer's plus Homebrew's),
# the default search would fill a module missing from the primary installation with the
# other Qt's build, and a binary linking two Qt runtimes aborts at launch.
get_filename_component(MYCEL_QT_CMAKE_DIR "${Qt6_DIR}/.." ABSOLUTE)
find_package(Qt6Quick QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)
find_package(Qt6QuickWidgets QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)
find_package(Qt6PrintSupport QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)
find_package(Qt6WebChannel QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)
find_package(Qt6Positioning QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)
find_package(Qt6WebEngineCore QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)
find_package(Qt6WebEngineWidgets QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)
find_package(Qt6Pdf QUIET CONFIG PATHS "${MYCEL_QT_CMAKE_DIR}" NO_DEFAULT_PATH)

add_executable(mycel
    src/main.cpp
    src/main_window_core.cpp
    src/main_window_persistence.cpp
    src/main_window_filesystem.cpp
    src/main_window_layout.cpp
    src/main_window_history.cpp
    src/main_window_links.cpp
    src/main_window_dragdrop.cpp
    src/main_window_actions.cpp
    src/main_window_preview.cpp
    src/main_window_board.cpp
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

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS mycel RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/mycel.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/mycel.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
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

# Packaging (Linux .deb). Run with: scripts/package-linux.sh
if(UNIX AND NOT APPLE)
    set(CPACK_PACKAGE_NAME "mycel")
    set(CPACK_PACKAGE_VENDOR "Noriaki Fukuyori")
    set(CPACK_PACKAGE_CONTACT "Noriaki Fukuyori <fukuyori.n@gmail.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Mind-map tool that uses folders and files as its data source")
    set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/fukuyori/Mycel")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_STRIP_FILES ON)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_NAME "mycel")
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    # Derive Depends: from the linked shared libraries (needs dpkg-dev installed).
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    include(CPack)
endif()
