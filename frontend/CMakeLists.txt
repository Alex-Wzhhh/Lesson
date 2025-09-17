cmake_minimum_required(VERSION 3.21)
project(Lesson LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 关键：让 MOC/UIC 自动开启
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOGEN_VERBOSE ON)   # 可观察 MOC 过程（调试用，可留可删）

set(CMAKE_AUTOUIC_SEARCH_PATHS
    "${CMAKE_CURRENT_SOURCE_DIR}"       # ← 新增：frontend/
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"    # 可留作兼容
)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui Network Concurrent)

# 一定要把 .h 也放进 target 源列表中，AUTOMOC 才会处理
qt_add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.cpp
    src/MainWindow.h
    src/WSIHandler.cpp
    src/WSIHandler.h
    src/WSIView.cpp
    src/WSIView.h
    src/DetectionResult.cpp
    src/DetectionResult.h
    src/HttpClient.cpp
    src/HttpClient.h
    src/InferenceClient.cpp
    src/InferenceClient.h
    src/MiniMapWidget.cpp
    src/MiniMapWidget.h
    src/wsiviewer.h

    mainwindow.ui
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_BINARY_DIR}  # ui_*.h / moc_*.cpp 生成在 build 目录
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Gui Qt6::Network Qt6::Concurrent)

if(OPENSLIDE_LIBRARY)
  target_include_directories(${PROJECT_NAME} PRIVATE ${OPENSLIDE_INCLUDE_DIR})
  target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENSLIDE_LIBRARY})
endif()
