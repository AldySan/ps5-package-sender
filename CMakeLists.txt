cmake_minimum_required(VERSION 3.21)

project(ps5-package-sender
    VERSION 0.0.2
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
    src/core/ScanPaths.h
    src/core/ScanPaths.cpp
    src/core/UpdateChecker.h
    src/core/UpdateChecker.cpp
    src/core/Discovery.h
    src/core/Discovery.cpp
    src/core/RangeFileServer.h
    src/core/RangeFileServer.cpp
    src/core/QueueModel.h
    src/core/QueueModel.cpp
    src/core/TransferQueue.h
    src/core/TransferQueue.cpp
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
    src/main.cpp
    resources/app-icon.qrc)

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
        qml/components/StatusBar.qml
        qml/components/QueueRow.qml
        qml/components/DestinationPicker.qml
        qml/pages/LibraryPage.qml
        qml/pages/QueuePage.qml
        qml/pages/HistoryPage.qml
        qml/pages/SettingsPage.qml)

set_target_properties(ps5-package-sender PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})

if(WIN32)
    file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/resources/app-icon.ico"
         DESTINATION "${CMAKE_CURRENT_BINARY_DIR}/resources")
    configure_file(resources/app-icon.rc.in
        "${CMAKE_CURRENT_BINARY_DIR}/resources/app-icon.rc" @ONLY)
    set_target_properties(ps5-package-sender PROPERTIES
        WIN32_EXECUTABLE TRUE)
    target_sources(ps5-package-sender PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/resources/app-icon.rc")
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
    add_executable(fileservertest tests/fileservertest.cpp)
    target_link_libraries(fileservertest PRIVATE ps5core)
    add_test(NAME fileservertest COMMAND fileservertest)
    add_executable(queuetest tests/queuetest.cpp)
    target_link_libraries(queuetest PRIVATE ps5core)
    add_test(NAME queuetest COMMAND queuetest)
    add_executable(scanpathstest tests/scanpathstest.cpp)
    target_link_libraries(scanpathstest PRIVATE ps5core)
    add_test(NAME scanpathstest COMMAND scanpathstest)
    add_executable(updatecheckertest tests/updatecheckertest.cpp)
    target_link_libraries(updatecheckertest PRIVATE ps5core)
    add_test(NAME updatecheckertest COMMAND updatecheckertest)
endif()
