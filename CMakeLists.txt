cmake_minimum_required(VERSION 3.16)

project(Qt4test VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置 Qt 路径
set(CMAKE_PREFIX_PATH "/usr/local/opt/qt@6")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

# 源文件
set(SOURCES
    main.cpp
    mainwindow.cpp
    widgets/videogen.cpp
    widgets/imagegen.cpp
)

# 头文件
set(HEADERS
    mainwindow.h
    widgets/videogen.h
    widgets/imagegen.h
)

# 资源文件
set(RESOURCES
    resources.qrc
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
    ${RESOURCES}
)

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
)

# macOS 特定设置
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.qt4test.app
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    )
endif()

# Windows 特定设置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "Qt4test"
    )

    # 设置应用图标
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
        target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    endif()
endif()
