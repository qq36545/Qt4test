cmake_minimum_required(VERSION 3.16)

file(READ "${CMAKE_SOURCE_DIR}/version.txt" APP_VERSION_RAW)
string(STRIP "${APP_VERSION_RAW}" APP_VERSION)
project(ChickenAI VERSION ${APP_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 设置 Qt 路径
set(CMAKE_PREFIX_PATH "/usr/local/opt/qt@6")

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui Sql Network Multimedia)

# 源文件
set(SOURCES
    main.cpp
    mainwindow.cpp
    widgets/videogen.cpp
    widgets/imagegen.cpp
    widgets/configwidget.cpp
    widgets/aboutwidget.cpp
    database/dbmanager.cpp
    network/videoapi.cpp
    network/imageuploader.cpp
    network/taskpollmanager.cpp
    network/updatemanager.cpp
)

# 头文件
set(HEADERS
    mainwindow.h
    widgets/videogen.h
    widgets/imagegen.h
    widgets/configwidget.h
    widgets/aboutwidget.h
    database/dbmanager.h
    network/videoapi.h
    network/imageuploader.h
    network/taskpollmanager.h
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

target_compile_definitions(${PROJECT_NAME} PRIVATE APP_VERSION_STR="${PROJECT_VERSION}")

target_link_libraries(${PROJECT_NAME}
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::Sql
    Qt6::Network
    Qt6::Multimedia
)

# macOS 特定设置
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER com.chickenai.app
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
        MACOSX_BUNDLE_ICON_FILE app.icns
    )

    # 复制 ICNS 文件到 bundle
    set_source_files_properties(resources/app.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION Resources
    )
    target_sources(${PROJECT_NAME} PRIVATE resources/app.icns)

    # 修正 macOS bundle 运行时库路径，避免同时加载 Homebrew Qt 与包内 Qt
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt)
    if(MACDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND /bin/sh -c "rm -rf '$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Frameworks' '$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/PlugIns' '$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>/Contents/Resources/qml'"
            COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:${PROJECT_NAME}>" -always-overwrite -verbose=0
            COMMAND /usr/bin/install_name_tool -change /usr/local/opt/qtbase/lib/QtWidgets.framework/Versions/A/QtWidgets @executable_path/../Frameworks/QtWidgets.framework/Versions/A/QtWidgets "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMAND /usr/bin/install_name_tool -change /usr/local/opt/qtbase/lib/QtSql.framework/Versions/A/QtSql @executable_path/../Frameworks/QtSql.framework/Versions/A/QtSql "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMAND /usr/bin/install_name_tool -change /usr/local/opt/qtmultimedia/lib/QtMultimedia.framework/Versions/A/QtMultimedia @executable_path/../Frameworks/QtMultimedia.framework/Versions/A/QtMultimedia "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMAND /usr/bin/install_name_tool -change /usr/local/opt/qtbase/lib/QtGui.framework/Versions/A/QtGui @executable_path/../Frameworks/QtGui.framework/Versions/A/QtGui "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMAND /usr/bin/install_name_tool -change /usr/local/opt/qtbase/lib/QtNetwork.framework/Versions/A/QtNetwork @executable_path/../Frameworks/QtNetwork.framework/Versions/A/QtNetwork "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMAND /usr/bin/install_name_tool -change /usr/local/opt/qtbase/lib/QtCore.framework/Versions/A/QtCore @executable_path/../Frameworks/QtCore.framework/Versions/A/QtCore "$<TARGET_FILE:${PROJECT_NAME}>"
            COMMAND /bin/sh -c "/usr/bin/install_name_tool -delete_rpath /usr/local/opt/qt@6/lib '$<TARGET_FILE:${PROJECT_NAME}>' || true"
            VERBATIM
        )
    endif()
endif()

# Windows 特定设置
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
        OUTPUT_NAME "ChickenAI"
    )

    # 设置应用图标
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
        target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc")
    endif()
endif()
