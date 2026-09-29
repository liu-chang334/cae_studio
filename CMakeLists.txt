# 设置最低 CMake 版本
cmake_minimum_required(VERSION 3.16)

# 设置项目名称和语言
project(CAEStudio LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

# 设置编译选项, 防止中文乱码
if(MSVC)
    add_compile_options(/utf-8)
endif()
        
# 添加第三方库根目录到 CMake 搜索路径
set(CMAKE_PREFIX_PATH 
  "D:/Qt/6.9.1/msvc2022_64"         # Qt
  "D:/3rd/VTK-9.5.0/install")       # VTK

# 查找 Qt 库
find_package(Qt6 CONFIG REQUIRED)
find_package(VTK CONFIG REQUIRED)
message(STATUS "Qt6_DIR: ${Qt6_DIR}")
message(STATUS "VTK_DIR: ${VTK_DIR}")

# 添加可执行文件
add_executable(CAEStudio 
  main.cpp
  MainWindow.cpp
)

# 添加头文件搜索路径
target_include_directories(CAEStudio PRIVATE
  ${CMAKE_SOURCE_DIR})

# 链接 Qt + VTK
target_link_libraries(CAEStudio PRIVATE
  Qt6::Widgets
  Qt6::OpenGLWidgets)

# 检查生成平台
if (MSVC AND CMAKE_GENERATOR_PLATFORM)
  message(STATUS "Generator platform: ${CMAKE_GENERATOR_PLATFORM}")
endif()