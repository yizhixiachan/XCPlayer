cmake_minimum_required(VERSION 3.16)

project(XCPlayer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FFMPEG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg-7.1.1")

find_package(Qt6 REQUIRED COMPONENTS Quick Sql QuickControls2)

qt_standard_project_setup(REQUIRES 6.8)

qt_add_resources(RESOURCE_FILE assets.qrc)

qt_add_executable(appXCPlayer
    main.cpp
    app.rc
    ${RESOURCE_FILE}
)

qt_add_qml_module(appXCPlayer
    URI XCPlayer
    VERSION 1.0
    QML_FILES
    qml/Main.qml
    SOURCES src/quickitem/VideoDisplay.h src/quickitem/VideoDisplay.cpp
    SOURCES src/core/XCAudioEngine.cpp src/core/XCAudioEngine.h src/core/XCMediaCore.cpp src/core/XCMediaCore.h src/core/XCType.h
    SOURCES src/XCPlayer.cpp src/XCPlayer.h
    SOURCES src/manager/CoverManager.cpp src/manager/CoverManager.h src/manager/DatabaseManager.cpp src/manager/DatabaseManager.h
    SOURCES src/utility/MediaInfo.h src/utility/MediaTool.cpp src/utility/MediaTool.h src/utility/ThreadPool.h src/utility/XCFL.cpp src/utility/XCFL.h
    QML_FILES qml/CtrlBar.qml qml/TitleBar.qml
    QML_FILES qml/MyButton.qml qml/FailedDialog.qml qml/MyListView.qml qml/NavButton.qml
    SOURCES src/platform/NativeEventFilter.cpp src/platform/NativeEventFilter.h
    SOURCES src/model/MediaModel.h src/model/MediaModel.cpp
    SOURCES src/model/SortFilterModel.h src/model/SortFilterModel.cpp
    QML_FILES qml/NavPanel.qml
    QML_FILES qml/MediaView.qml
    QML_FILES qml/PlaylistView.qml
    SOURCES src/model/PlaylistModel.h src/model/PlaylistModel.cpp
    QML_FILES qml/FloatingWindow.qml
    QML_FILES qml/MyMenuItem.qml
    QML_FILES qml/AudioDelegate.qml
    QML_FILES qml/VideoDelegate.qml
    QML_FILES qml/AudioDisplay.qml
    SOURCES src/model/LyricsModel.h src/model/LyricsModel.cpp
    QML_FILES qml/MediaInfoPopup.qml
    QML_FILES qml/ConfirmDialog.qml
    QML_FILES qml/SettingView.qml
    QML_FILES
    QML_FILES qml/ArtistView.qml
    SOURCES src/model/AlbumModel.h src/model/AlbumModel.cpp
    QML_FILES qml/ToastDialog.qml
    QML_FILES qml/FloatingLyrics.qml
    QML_FILES qml/MetadataPopup.qml
    QML_FILES qml/ToolView.qml
    QML_FILES qml/LibraryView.qml
    QML_FILES qml/NetworkStreamView.qml

)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(appXCPlayer PROPERTIES
    #    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.appXCPlayer
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_include_directories(appXCPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/model
    ${CMAKE_CURRENT_SOURCE_DIR}/src/quickitem
    ${FFMPEG_DIR}/include
)

target_link_libraries(appXCPlayer PRIVATE
    Qt6::Quick
    Qt6::Sql
    Qt6::QuickControls2

    ${FFMPEG_DIR}/lib/avcodec.lib
    ${FFMPEG_DIR}/lib/avfilter.lib
    ${FFMPEG_DIR}/lib/avformat.lib
    ${FFMPEG_DIR}/lib/avutil.lib
    ${FFMPEG_DIR}/lib/swresample.lib
    ${FFMPEG_DIR}/lib/swscale.lib

    d3d11
    dxgi

    winmm
    Avrt
    ksguid
)

include(GNUInstallDirs)
install(TARGETS appXCPlayer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
