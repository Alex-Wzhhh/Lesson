cmake_minimum_required(VERSION 3.21)

project(Lesson LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_PREFIX_PATH "D:/University/Qt/6.9.1/mingw_64")

find_package(QT NAMES Qt6 REQUIRED COMPONENTS Widgets Network Qt6Core)
find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    imagelabel.cpp
    imagelabel.h
    wsiviewer.cpp
    wsiviewer.h
    mainwindow.ui
)

add_executable(QtPythonHybrid
    ${PROJECT_SOURCES}
)
target_link
_libraries(QtPythonHybrid PRIVATE Qt6::Widgets Qt6::Network)
