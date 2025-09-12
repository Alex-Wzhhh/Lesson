cmake_minimum_required(VERSION 3.21)
project(WSIFrontend LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 如果你使用 vcpkg，请在 CMake 配置时通过 -DCMAKE_TOOLCHAIN_FILE 传入
# set(CMAKE_TOOLCHAIN_FILE "D:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Network)

# 可选：OpenSlide（用于读取 .svs 等 WSI 文件）
find_path(OPENSLIDE_INCLUDE_DIR openslide/openslide.h)
find_library(OPENSLIDE_LIBRARY NAMES openslide)
if(OPENSLIDE_INCLUDE_DIR AND OPENSLIDE_LIBRARY)
    message(STATUS "Found OpenSlide: ${OPENSLIDE_LIBRARY}")
    add_compile_definitions(USE_OPENSLIDE)
endif()

set(SRC
    src/main.cpp
    src/MainWindow.cpp
    src/WSIHandler.cpp
    src/WSIView.cpp
    src/DetectionResult.cpp
    src/HttpClient.cpp
    src/InferenceClient.cpp
)

set(HDR
    src/MainWindow.h
    src/WSIHandler.h
    src/WSIView.h
    src/DetectionResult.h
    src/HttpClient.h
    src/InferenceClient.h
)

set(UI
    ui/mainwindow.ui
)

qt_wrap_ui(UI_HEADERS ${UI})

add_executable(${PROJECT_NAME} ${SRC} ${HDR} ${UI} ${UI_HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE src ${CMAKE_CURRENT_BINARY_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Gui Qt6::Network)

if(OPENSLIDE_LIBRARY)
    target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSLIDE_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSLIDE_LIBRARY})
else()
    message(WARNING "OpenSlide not found. 将以普通大图像模式运行（DUMMY_WSI）。")
    add_compile_definitions(DUMMY_WSI)
endif()

# Windows: 复制 Qt 依赖可使用 windeployqt（手动）
