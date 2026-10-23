cmake_minimum_required(VERSION 3.21)
project(BaseVision VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()
if(WIN32)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)
find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc videoio)
find_package(Threads REQUIRED)

add_library(vision_image STATIC src/ImageProcessor.cpp)
target_include_directories(vision_image PUBLIC src ${OpenCV_INCLUDE_DIRS})
target_link_libraries(vision_image PUBLIC ${OpenCV_LIBS})

add_library(vision_camera STATIC
    src/CameraDevices.cpp src/CameraDevices.h
    src/CameraEngine.cpp src/CameraEngine.h
    src/HikrobotCamera.cpp src/HikrobotCamera.h)
target_link_libraries(vision_camera PUBLIC vision_image Qt6::Gui Threads::Threads)

option(BASEVISION_ENABLE_MVS "Enable Hikrobot USB cameras when MVS headers are installed" ON)
set(MVS_ROOT "" CACHE PATH "Hikrobot MVS SDK root (contains include/ or Includes/)")
if(BASEVISION_ENABLE_MVS)
    find_path(MVS_INCLUDE_DIR MvCameraControl.h
        HINTS "${MVS_ROOT}" "$ENV{MVCAM_SDK_PATH}"
        PATHS /opt/MVS "C:/Program Files (x86)/MVS/Development"
        PATH_SUFFIXES include Includes)
    if(MVS_INCLUDE_DIR)
        target_include_directories(vision_camera SYSTEM PRIVATE "${MVS_INCLUDE_DIR}")
        target_compile_definitions(vision_camera PRIVATE BASEVISION_HAS_MVS=1)
        # Runtime loading keeps normal webcams usable without an installed MVS runtime.
        find_library(MVS_RUNTIME_LIBRARY NAMES MvCameraControl
            HINTS "${MVS_ROOT}" "$ENV{MVCAM_SDK_PATH}"
            PATHS /opt/MVS PATH_SUFFIXES lib/64 lib/aarch64)
        if(MVS_RUNTIME_LIBRARY AND NOT WIN32)
            target_compile_definitions(vision_camera PRIVATE
                BASEVISION_MVS_LIBRARY_HINT="${MVS_RUNTIME_LIBRARY}")
        endif()
        message(STATUS "Hikrobot USB support enabled (MVS runtime loaded on demand)")
    else()
        message(STATUS "MVS SDK headers not found; building with system webcams only")
    endif()
endif()
if(WIN32)
    target_link_libraries(vision_camera PRIVATE ole32 oleaut32 strmiids)
endif()

add_library(vision_ui STATIC
    src/MainWindow.cpp src/MainWindow.h
    src/PreviewWidget.cpp src/PreviewWidget.h)
target_link_libraries(vision_ui PUBLIC vision_camera Qt6::Widgets)

add_executable(base_vision WIN32 src/main.cpp)
target_link_libraries(base_vision PRIVATE vision_ui)

option(BASEVISION_BUILD_DEMOS "Build reproducible synthetic rule comparison generator" ON)
if(BASEVISION_BUILD_DEMOS)
    add_executable(rule_demo tools/rule_demo.cpp)
    target_link_libraries(rule_demo PRIVATE vision_image Qt6::Gui)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(image_tests tests/image_tests.cpp)
    target_link_libraries(image_tests PRIVATE vision_image)
    add_test(NAME image_rules COMMAND image_tests)

    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)
    add_executable(ui_tests tests/ui_tests.cpp)
    target_link_libraries(ui_tests PRIVATE vision_ui Qt6::Test)
    add_test(NAME desktop_controls COMMAND ui_tests)
    set_tests_properties(desktop_controls PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME desktop_high_dpi COMMAND ui_tests)
    set_tests_properties(desktop_high_dpi PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_SCALE_FACTOR=2")

    # Hardware tests are deliberately opt-in, never run by ordinary ctest.
    add_executable(camera_smoke tests/camera_smoke.cpp)
    target_link_libraries(camera_smoke PRIVATE vision_camera Qt6::Core)
endif()

# On Windows, resolve the actual runtime dependencies of this build, including
# OpenCV DLLs. Qt's plugins/platforms are added by scripts/deploy-windows.ps1.
if(WIN32)
    set(BASEVISION_RUNTIME_DIRS "" CACHE STRING "Qt/OpenCV runtime DLL search directories")
    install(TARGETS base_vision RUNTIME_DEPENDENCY_SET vision_runtime
        RUNTIME DESTINATION .)
    install(RUNTIME_DEPENDENCY_SET vision_runtime
        RUNTIME DESTINATION .
        DIRECTORIES ${BASEVISION_RUNTIME_DIRS}
        PRE_EXCLUDE_REGEXES "api-ms-.*" "ext-ms-.*"
        POST_EXCLUDE_REGEXES ".*[Ww][Ii][Nn][Dd][Oo][Ww][Ss][/\\\\][Ss][Yy][Ss][Tt][Ee][Mm]32[/\\\\].*")
else()
    install(TARGETS base_vision RUNTIME DESTINATION .)
endif()
