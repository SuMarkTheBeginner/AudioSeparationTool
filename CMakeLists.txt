cmake_minimum_required(VERSION 3.5)

project(AudioSeparationTool VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

### 🔹 加入 LibTorch
# 修改這裡成你的 libtorch 解壓路徑
set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};$ENV{HOME}/libtorch")
find_package(Torch REQUIRED)

### 🔹 用 pkg-config 找 libsndfile
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)
pkg_check_modules(SAMPLERATE REQUIRED samplerate)

set(TS_FILES AudioSeparationTool_zh_TW.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        mainwindow.ui
        ${TS_FILES}
        addsoundfeaturewidget.h addsoundfeaturewidget.cpp
        fileutils.h fileutils.cpp
        filewidget.h filewidget.cpp
        widecheckbox.h
        folderwidget.h folderwidget.cpp
        htsatprocessor.h htsatprocessor.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AudioSeparationTool
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        .gitignore
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(AudioSeparationTool SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(AudioSeparationTool
            ${PROJECT_SOURCES}
        )
    endif()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

### 🔹 連結 Qt + LibTorch + libsndfile
target_include_directories(AudioSeparationTool PRIVATE ${SNDFILE_INCLUDE_DIRS} ${SAMPLERATE_INCLUDE_DIRS})
target_link_libraries(AudioSeparationTool
    PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    ${TORCH_LIBRARIES}
    ${SNDFILE_LIBRARIES}
    ${SAMPLERATE_LIBRARIES}
)

# 避免 LibTorch 一些標準差異警告
set_property(TARGET AudioSeparationTool PROPERTY CXX_STANDARD 17)

if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.AudioSeparationTool)
endif()
set_target_properties(AudioSeparationTool PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS AudioSeparationTool
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(AudioSeparationTool)
endif()
