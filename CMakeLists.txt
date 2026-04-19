cmake_minimum_required(VERSION 3.5)

project(NetherLink-static VERSION 0.1 LANGUAGES CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)

include_directories(${PROJECT_SOURCE_DIR}/components/include)
include_directories(${PROJECT_SOURCE_DIR}/utils/include)
include_directories(${PROJECT_SOURCE_DIR}/view/mainwindow/include)
include_directories(${PROJECT_SOURCE_DIR}/view/aichat/include)
include_directories(${PROJECT_SOURCE_DIR}/view/chat/include)
include_directories(${PROJECT_SOURCE_DIR}/view/post/include)
include_directories(${PROJECT_SOURCE_DIR}/view/friend/include)
include_directories(${PROJECT_SOURCE_DIR}/data/include)
include_directories(${PROJECT_SOURCE_DIR}/entity)

file(GLOB COMPONENTS_SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/components/src/*.cpp")
file(GLOB COMPONENTS_HEADERS  "${CMAKE_CURRENT_SOURCE_DIR}/components/include/*.h")
file(GLOB UTILS_SOURCES       "${CMAKE_CURRENT_SOURCE_DIR}/utils/src/*.cpp")
file(GLOB UTILS_HEADERS       "${CMAKE_CURRENT_SOURCE_DIR}/utils/include/*.h")
file(GLOB ENTITY_HEADERS      "${CMAKE_CURRENT_SOURCE_DIR}/entity/*.h")
file(GLOB DATA_HEADERS        "${CMAKE_CURRENT_SOURCE_DIR}/data/include/*.h")
file(GLOB DATA_SOURCES        "${CMAKE_CURRENT_SOURCE_DIR}/data/src/*.cpp")

file(GLOB VIEW_AICHAT_HEADERS     "${CMAKE_CURRENT_SOURCE_DIR}/view/aichat/include/*.h")
file(GLOB VIEW_AICHAT_SOURCES     "${CMAKE_CURRENT_SOURCE_DIR}/view/aichat/src/*.cpp")
file(GLOB VIEW_CHAT_HEADERS       "${CMAKE_CURRENT_SOURCE_DIR}/view/chat/include/*.h")
file(GLOB VIEW_CHAT_SOURCES       "${CMAKE_CURRENT_SOURCE_DIR}/view/chat/src/*.cpp")
file(GLOB VIEW_FRIEND_HEADERS     "${CMAKE_CURRENT_SOURCE_DIR}/view/friend/include/*.h")
file(GLOB VIEW_FRIEND_SOURCES     "${CMAKE_CURRENT_SOURCE_DIR}/view/friend/src/*.cpp")
file(GLOB VIEW_POST_HEADERS       "${CMAKE_CURRENT_SOURCE_DIR}/view/post/include/*.h")
file(GLOB VIEW_POST_SOURCES       "${CMAKE_CURRENT_SOURCE_DIR}/view/post/src/*.cpp")
file(GLOB VIEW_MAINWINDOW_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/view/mainwindow/include/*.h")
file(GLOB VIEW_MAINWINDOW_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/view/mainwindow/src/*.cpp")

set(PLATFORM_SOURCES)
if(APPLE)
    list(APPEND PLATFORM_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/utils/src/MacWindowBridge.mm
    )
endif()

set(PROJECT_SOURCES
    main.cpp
    resources.qrc
    ${COMPONENTS_HEADERS}
    ${COMPONENTS_SOURCES}
    ${UTILS_SOURCES}
    ${UTILS_HEADERS}
    ${ENTITY_HEADERS}
    ${DATA_HEADERS}
    ${DATA_SOURCES}
    ${VIEW_AICHAT_HEADERS}
    ${VIEW_AICHAT_SOURCES}
    ${VIEW_CHAT_HEADERS}
    ${VIEW_CHAT_SOURCES}
    ${VIEW_FRIEND_HEADERS}
    ${VIEW_FRIEND_SOURCES}
    ${VIEW_POST_HEADERS}
    ${VIEW_POST_SOURCES}
    ${VIEW_MAINWINDOW_HEADERS}
    ${VIEW_MAINWINDOW_SOURCES}
    ${PLATFORM_SOURCES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(NetherLink-static
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        utils/include/imagemanager.h utils/include/imagemanager.cpp
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET NetherLink-static APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(NetherLink-static SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(NetherLink-static
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_link_libraries(NetherLink-static PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

if(WIN32)
    target_link_libraries(NetherLink-static PRIVATE dwmapi user32)
endif()

set_target_properties(NetherLink-static PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER my.example.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS NetherLink-static
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(NetherLink-static)
endif()
