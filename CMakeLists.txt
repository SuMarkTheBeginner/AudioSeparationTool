cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0148 OLD)
project(AudioSeparationTool VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    add_compile_options(/W1 /wd4267)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
endif()

option(FORCE_CPU_ONLY "Force CPU-only build, disable CUDA" ON)

# ---- vcpkg toolchain ----
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
endif()

# ---- Qt ----
set(Qt6_DIR "D:/Qt/6.9.2/msvc2022_64/lib/cmake/Qt6")
find_package(Qt6 REQUIRED COMPONENTS Widgets LinguistTools Multimedia)

set(Python3_EXECUTABLE "C:/Users/aaa09/.conda/envs/torchscript_env/python.exe")
find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_definitions(-DHAS_MULTIMEDIA)

# ---- LibTorch ----
set(Torch_DIR "C:/libtorch/share/cmake/Torch")
set(CMAKE_PREFIX_PATH "D:/Qt/6.9.2/msvc2022_64;C:/libtorch;C:/vcpkg/installed/x64-windows")
set(CAFFE2_USE_CUDA OFF)
set(TORCH_CUDA_FOUND OFF)
find_package(Torch REQUIRED PATHS "C:/libtorch" NO_DEFAULT_PATH)
include_directories(${TORCH_INCLUDE_DIRS})

# ---- Project sources ----
set(TS_FILES AudioSeparationTool_zh_TW.ts)

set(MODELS_QRC ${CMAKE_SOURCE_DIR}/models.qrc)
set_source_files_properties(${MODELS_QRC} PROPERTIES SKIP_AUTOGEN ON)

find_program(QT_RCC_EXECUTABLE NAMES rcc HINTS "D:/Qt/6.9.2/msvc2022_64/bin" PATH_SUFFIXES bin)
if(NOT QT_RCC_EXECUTABLE)
    message(FATAL_ERROR "Could not find rcc executable. Set proper Qt path.")
endif()

set(MODELS_RCC ${CMAKE_BINARY_DIR}/models.rcc)

add_custom_command(
    OUTPUT ${MODELS_RCC}
    COMMAND ${QT_RCC_EXECUTABLE} -binary ${MODELS_QRC} -o ${MODELS_RCC}
    DEPENDS ${MODELS_QRC}
    COMMENT "Generating binary models.rcc from models.qrc"
    VERBATIM
)

add_custom_target(models_rcc_target ALL DEPENDS ${MODELS_RCC})

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    ${TS_FILES}
    constants.h
    errorhandler.cpp
    errorhandler.h
    addsoundfeaturewidget.cpp
    addsoundfeaturewidget.h
    usefeaturewidget.cpp
    usefeaturewidget.h
    fileutils.cpp
    fileutils.h
    filemanagerwidget.cpp
    filemanagerwidget.h
    filewidget.cpp
    filewidget.h
    widecheckbox.cpp
    widecheckbox.h
    folderwidget.cpp
    folderwidget.h
    htsatprocessor.cpp
    htsatprocessor.h
    zero_shot_asp_feature_extractor.cpp
    zero_shot_asp_feature_extractor.h
    htsatworker.cpp
    htsatworker.h
    separationworker.cpp
    separationworker.h
    audio_preprocess_utils.cpp
    audio_preprocess_utils.h
    resourcemanager.cpp
    resourcemanager.h
    audioplayer.cpp
    audioplayer.h
    logger.cpp
    logger.h
    filelocker.cpp
    filelocker.h
    audioserializer.cpp
    audioserializer.h
    filerepo.cpp
    filerepo.h
    asyncprocessor.cpp
    asyncprocessor.h
)

# ---- build target ----
qt_add_executable(AudioSeparationTool
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

add_dependencies(AudioSeparationTool models_rcc_target)

add_custom_command(TARGET AudioSeparationTool POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${MODELS_RCC}
            $<TARGET_FILE_DIR:AudioSeparationTool>
)

# ---- Set environment variable to disable QML JIT ----
set_target_properties(AudioSeparationTool PROPERTIES
    ENVIRONMENT "QML_DISABLE_OPTIMIZER=1"
)

# ---- link libraries ----
find_package(SndFile REQUIRED)
find_package(SampleRate CONFIG REQUIRED)

target_link_libraries(AudioSeparationTool
    PRIVATE
    Qt6::Widgets
    Qt6::Multimedia
    torch
    SampleRate::samplerate
    SndFile::sndfile
)





qt_finalize_executable(AudioSeparationTool)
