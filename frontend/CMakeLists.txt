cmake_minimum_required(VERSION 3.21)

# 工程名
project(Lesson LANGUAGES CXX)

# Qt 自动化特性
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 你用的是 Qt6 Widgets
find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# 建议 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 把你的真实相对路径写在这里 ----
set(
    main.cpp
    mainWindow.cpp
    mainWindow.h
    mainWindow.ui

)
# -------------------------------------

# 可执行目标
add_executable(QtPythonHybrid)

# 连接 Qt Widgets
target_link_libraries(QtPythonHybrid PRIVATE Qt6::Widgets)
