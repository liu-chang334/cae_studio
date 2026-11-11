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
find_package(Qt6 CONFIG REQUIRED COMPONENTS
  Widgets
  OpenGLWidgets
)
find_package(VTK CONFIG REQUIRED COMPONENTS
  CommonCore
  FiltersCore # 提取单元与网格边线
  GUISupportQt
  RenderingOpenGL2
  InteractionWidgets
  RenderingAnnotation
)
message(STATUS "Qt6_DIR: ${Qt6_DIR}")
message(STATUS "VTK_DIR: ${VTK_DIR}")

# 将公共矩阵运算编译成静态库
add_library(CAEMath STATIC src/MatrixMath.cpp)

# 让CAEMath及链接它的目标能找到公开头文件
target_include_directories(CAEMath PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

# 数学库关闭该目标的AUTOMOC
set_target_properties(CAEMath PROPERTIES AUTOMOC OFF)

# 添加可执行文件
add_executable(CAEStudio 
  main.cpp
  src/MainWindow.cpp
  src/vtkScene.cpp
  src/MeshViewport.cpp
  src/AbaqusInpReader.cpp
  src/vtkBuilder.cpp
  src/C3D8Element.cpp # C3D8单元几何计算实现
)

# 添加头文件搜索路径
target_include_directories(CAEStudio PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# 将含有 Q_OBJECT 的头文件加入目标，供 AUTOMOC 扫描
target_sources(CAEStudio PRIVATE
    include/MainWindow.h
    include/MeshViewport.h
)

# 链接 Qt + VTK
target_link_libraries(CAEStudio PRIVATE
  CAEMath # 链接公共矩阵运算库
  
  Qt6::Widgets
  Qt6::OpenGLWidgets
  VTK::GUISupportQt
  VTK::RenderingOpenGL2
  VTK::InteractionWidgets
  VTK::RenderingAnnotation
  VTK::FiltersCore # 链接单元与边线提取模块
)

# 检查生成平台
if (MSVC AND CMAKE_GENERATOR_PLATFORM)
  message(STATUS "Generator platform: ${CMAKE_GENERATOR_PLATFORM}")
endif()
