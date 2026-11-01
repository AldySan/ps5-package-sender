cmake_minimum_required(VERSION 3.21)

project(ps5-package-sender
    VERSION 0.1.0
    DESCRIPTION "PS5 package sender desktop client"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Quick QuickControls2 Sql Network Concurrent)

qt_standard_project_setup(REQUIRES 6.5)

add_library(ps5core STATIC
    src/core/PkgInfo.h
    src/core/PkgInfo.cpp
    src/core/ReceiverClient.h
    src/core/ReceiverClient.cpp
    src/core/LibraryScanner.h
    src/core/LibraryScanner.cpp
    src/core/LibraryModel.h
    src/core/LibraryModel.cpp
    src/db/Database.h
    src/db/Database.cpp
    src/util/Format.h)

target_include_directories(ps5core PUBLIC src)
target_link_libraries(ps5core PUBLIC Qt6::Core Qt6::Sql Qt6::Network Qt6::Concurrent)

qt_add_executable(ps5-package-sender
    src/main.cpp)

target_include_directories(ps5-package-sender PRIVATE src)

target_compile_definitions(ps5-package-sender PRIVATE
    APP_VERSION="${PROJECT_VERSION}")

target_link_libraries(ps5-package-sender PRIVATE
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Network
    Qt6::Concurrent
    ps5core)

set_source_files_properties(qml/Theme.qml PROPERTIES
    QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(ps5-package-sender
    URI PS5PackageSender
    VERSION 1.0
    SOURCES
        src/app/Application.h
        src/app/Application.cpp
        src/app/IconProvider.h
        src/app/IconProvider.cpp
    QML_FILES
        qml/Theme.qml
        qml/Main.qml
        qml/components/GameCard.qml
        qml/components/Toast.qml
        qml/pages/LibraryPage.qml
        qml/pages/QueuePage.qml
        qml/pages/HistoryPage.qml
        qml/pages/SettingsPage.qml)

set_target_properties(ps5-package-sender PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

if(WIN32)
    set_target_properties(ps5-package-sender PROPERTIES
        WIN32_EXECUTABLE TRUE)
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(ps5-package-sender PROPERTIES
        INSTALL_RPATH "$ORIGIN/lib")
endif()

if(BUILD_TESTS)
    enable_testing()
    add_executable(pkgparsertest tests/pkgparsertest.cpp)
    target_link_libraries(pkgparsertest PRIVATE ps5core)
    add_test(NAME pkgparsertest COMMAND pkgparsertest)
endif()

if(BUILD_TESTS)
    add_executable(librarytest tests/librarytest.cpp)
    target_link_libraries(librarytest PRIVATE ps5core)
    add_test(NAME librarytest COMMAND librarytest)
    add_executable(receivertest tests/receivertest.cpp)
    target_link_libraries(receivertest PRIVATE ps5core)
    add_test(NAME receivertest COMMAND receivertest)
endif()
