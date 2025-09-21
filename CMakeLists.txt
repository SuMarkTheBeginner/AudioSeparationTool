cmake_minimum_required(VERSION 3.5)

option(FORCE_CPU_ONLY "Force CPU-only build, disable CUDA" ON)

if(FORCE_CPU_ONLY)
    project(AudioSeparationTool VERSION 0.1 LANGUAGES CXX)
    set(USE_CUDA OFF CACHE BOOL "Use CUDA" FORCE)
    set(CAFFE2_USE_CUDA OFF CACHE BOOL "Use CUDA in Caffe2" FORCE)
    set(CAFFE2_USE_CUDNN OFF CACHE BOOL "Use CUDNN in Caffe2" FORCE)
    set(USE_CUDNN OFF CACHE BOOL "Use CUDNN" FORCE)
    set(USE_NCCL OFF CACHE BOOL "Use NCCL" FORCE)
else()
    project(AudioSeparationTool VERSION 0.1 LANGUAGES CXX CUDA)
    # Set compiler explicitly to GCC 11 for CUDA compatibility
    set(CMAKE_C_COMPILER "/usr/bin/gcc-11" CACHE STRING "C compiler" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/g++-11" CACHE STRING "C++ compiler" FORCE)
    set(CMAKE_CUDA_HOST_COMPILER "/usr/bin/gcc-11" CACHE STRING "CUDA host compiler" FORCE)
endif()

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

# Try to find Multimedia component
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Multimedia)
if(Qt${QT_VERSION_MAJOR}Multimedia_FOUND)
    set(HAS_MULTIMEDIA TRUE)
else()
    set(HAS_MULTIMEDIA FALSE)
    message(WARNING "Qt Multimedia not found. Audio playback will not be available.")
endif()

### 🔹 LibTorch
if(FORCE_CPU_ONLY)
    message(STATUS "Using CPU-only LibTorch")
    message(STATUS "Please download from: https://pytorch.org/get-started/locally/")
    message(STATUS "Select: LibTorch -> Stable -> Your OS -> C++/Java -> CPU")
else()
    message(STATUS "Using CUDA-enabled LibTorch for GPU support")
    message(STATUS "Please download from: https://pytorch.org/get-started/locally/")
    message(STATUS "Select: LibTorch -> Stable -> Your OS -> C++/Java -> CUDA -> cxx11 ABI")
endif()
message(STATUS "Extract and update this path:")

set(LIBTORCH_PATHS
    "/home/mark/libtorch-cpu"
    "$ENV{HOME}/libtorch"
)

set(LIBTORCH_FOUND FALSE)
foreach(LIBTORCH_PATH ${LIBTORCH_PATHS})
    if(EXISTS "${LIBTORCH_PATH}/share/cmake/Torch/TorchConfig.cmake")
        set(CMAKE_PREFIX_PATH "${CMAKE_PREFIX_PATH};${LIBTORCH_PATH}")
        set(LIBTORCH_FOUND TRUE)
        message(STATUS "Found LibTorch at: ${LIBTORCH_PATH}")
        break()
    endif()
endforeach()

if(NOT LIBTORCH_FOUND)
    if(FORCE_CPU_ONLY)
        message(FATAL_ERROR "LibTorch not found. Please download CPU-only LibTorch and extract to ~/libtorch")
    else()
        message(FATAL_ERROR "LibTorch not found. Please download CUDA-enabled LibTorch and extract to ~/libtorch")
    endif()
endif()

if(NOT FORCE_CPU_ONLY)
    # Enable CUDA - set these BEFORE find_package
    set(USE_CUDA ON CACHE BOOL "Use CUDA" FORCE)
    set(CAFFE2_USE_CUDA ON CACHE BOOL "Use CUDA in Caffe2" FORCE)
    set(CAFFE2_USE_CUDNN ON CACHE BOOL "Use CUDNN in Caffe2" FORCE)
    set(USE_CUDNN ON CACHE BOOL "Use CUDNN" FORCE)
    set(USE_NCCL ON CACHE BOOL "Use NCCL" FORCE)

    # Enable CUDA architecture detection
    set(TORCH_CUDA_ARCH_LIST "8.6" CACHE STRING "CUDA architectures" FORCE)
    set(CMAKE_CUDA_ARCHITECTURES "8.6" CACHE STRING "CUDA architectures" FORCE)

    # Do not remove CUDA from PATH and environment
    # Do not clear CUDA environment variables
    # Do not disable CUDA language completely
endif()

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
        constants.h
        errorhandler.h errorhandler.cpp
        addsoundfeaturewidget.h addsoundfeaturewidget.cpp
        usefeaturewidget.h usefeaturewidget.cpp
        fileutils.h fileutils.cpp
        filemanagerwidget.h filemanagerwidget.cpp
        filewidget.h filewidget.cpp
        widecheckbox.h widecheckbox.cpp
        folderwidget.h folderwidget.cpp
        htsatprocessor.h htsatprocessor.cpp
        zero_shot_asp_feature_extractor.h zero_shot_asp_feature_extractor.cpp
        htsatworker.h htsatworker.cpp
        separationworker.h separationworker.cpp
        audio_preprocess_utils.h audio_preprocess_utils.cpp
        # models.qrc  # Commented out - models are too large for Qt resource compiler
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(AudioSeparationTool
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        resourcemanager.h resourcemanager.cpp
        audioplayer.h audioplayer.cpp
        # models.qrc  # Commented out - models are too large for Qt resource compiler
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

if(HAS_MULTIMEDIA)
    target_link_libraries(AudioSeparationTool PRIVATE Qt${QT_VERSION_MAJOR}::Multimedia)
    target_compile_definitions(AudioSeparationTool PRIVATE HAS_MULTIMEDIA)
endif()

# 🔹 啟用 AddressSanitizer (ASan) - Disabled
# target_compile_options(AudioSeparationTool PRIVATE -fsanitize=address -g)
# target_link_options(AudioSeparationTool PRIVATE -fsanitize=address)

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


