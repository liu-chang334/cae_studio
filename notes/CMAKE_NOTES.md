<!-- omit from toc -->
# CMAKE_NOTES
- [1. CMake内置变量](#1-cmake内置变量)
- [2. add\_executable](#2-add_executable)
- [3. add\_library](#3-add_library)
- [4. cmake\_minimum\_required](#4-cmake_minimum_required)
- [5. foreach](#5-foreach)
- [6. message](#6-message)
- [7. project](#7-project)
- [8. set](#8-set)
- [9. set\_target\_properties](#9-set_target_properties)
- [10. target\_compile\_options](#10-target_compile_options)
- [11. target\_include\_directories](#11-target_include_directories)
- [12. target\_link\_libraries](#12-target_link_libraries)
- [13. target\_sources](#13-target_sources)
  
## 1. CMake内置变量

- `CMAKE_VERSION`, 当前CMake版本号
- `CMAKE_SOURCE_DIR`, 顶层源码目录, 即`cmake -S`指定的目录
- `CMAKE_BINARY_DIR`, 顶层构建目录, 即`cmake -B`指定的目录
- `CMAKE_CURRENT_SOURCE_DIR`, 当前正在处理的`CMakeLists.txt`所在的源码目录
- `CMAKE_CURRENT_BINARY_DIR`, 当前正在处理的`CMakeLists.txt`所在的构建目录

## 2. add_executable

定义可执行文件目标, 并指定生成该程序需要的源文件

基本语法:

```cmake
add_executable(
    <目标名>
    [WIN32]
    [MACOSX_BUNDLE]
    [EXCLUDE_FROM_ALL]
    <源文件>...
)
```

常见写法:

```cmake
# 一个源文件
add_executable(MyApp main.cpp)

# 多个源文件
add_executable(MyApp
    main.cpp
    utils.cpp
    calculator.cpp
)

# 先保存源文件列表
set(SOURCES
    main.cpp
    utils.cpp
    calculator.cpp
)
add_executable(MyApp ${SOURCES})
```

## 3. add_library

用于创建一个库目标

库用于封装可以被多个可执行程序或其他库重复使用的代码

基本语法:

```cmake
add_library(
    <目标名>
    [STATIC | SHARED | MODULE]
    [EXCLUDE_FROM_ALL]
    <源文件>...
)
```

### 3.1. 静态库:

```cmake
add_library(numerics STATIC
    src/vector.f90
    src/matrix.f90
)
add_executable(solver
    src/main.f90
)
target_link_libraries(solver PRIVATE
    numerics
)
```

静态库在链接阶段复制到最终可执行文件中, 更新静态库后需要重新链接可执行程序

不同平台生成的静态库文件不同:

```text
Windows: numerics.lib
Linux: libnumerics.a
MacOS: libnumerics.a
```

### 3.2. 动态库:

```cmake
add_library(numerics SHARED
    src/vector.f90
    src/matrix.f90
)
add_executable(solver
    src/main.f90
)
target_link_libraries(solver PRIVATE
    numerics
)
```

不同平台生成的动态库文件不同:

```text
Windows: numerics.dll
Linux: libnumerics.so
MacOS: libnumerics.dylib
```

动态库代码不会完整复制进可执行文件

多个程序可以共享同一个动态库

可以单独替换动态库, 而不需要重新链接可执行程序

### 3.3. 模块库

MODULE库用于运行时动态加载, 与SHARED的区别是:

- `SHARED`库供其他目标在链接阶段链接
- `MODULE`库供程序在运行时动态加载, 一般不使用`target_link_libraries()`
- `MODULE`库自身仍然可以链接其他依赖:

```cmake
add_library(material_plugin MODULE
    src/material_plugin.f90
)
target_link_libraries(material_plugin PRIVATE
    numerics
)
```

## 4. cmake_minimum_required

声明构建该项目所需的最低CMake版本

基本语法:

```cmake
cmake_minimum_required(
    VERSION <最低版本>[...<策略兼容版本>]
)
```

- <最低版本>: 构建项目所需的最低CMake版本

常见写法:

```cmake
cmake_minimum_required(VERSION 3.20)
```

- 必须放在顶层`CMakeLists.txt`最前面, 位于`project()`之前
  
## 5. foreach

## 6. message

## 7. project

基本语法:

```cmake
project(
    <项目名>
    [VERSION <主版本>[.<次版本>[.<补丁版本>[.<修订版本>]]]]
    [DESCRIPTION <项目描述>]
    [HOMEPAGE_URL <项目主页>]
    [LANGUAGES <语言列表>]
)
```

常见写法:

```cmake
project(MyProject LANGUAGES C CXX Fortran)
```

CMake会创建以下变量:

- `PROJECT_NAME`, 最近一次调用project()所定义的项目名, 这里为`MyProject`
- `PROJECT_SOURCE_DIR`, 当前项目的源码目录, 绝对路径
- `PROJECT_BINARY_DIR`, 当前项目的构建目录, 绝对路径

此外, CMake会检测对应编译器并创建语言相关变量:

- `CMAKE_C_COMPILER`, C编译器可执行文件路径
- `CMAKE_C_COMPILER_ID`, C编译器类型/厂商标识
- `CMAKE_C_COMPILER_VERSION`, C编译器版本
- `CMAKE_CXX_COMPILER`, C++编译器可执行文件路径
- `CMAKE_CXX_COMPILER_ID`, C++编译器类型/厂商标识
- `CMAKE_CXX_COMPILER_VERSION`, C++编译器版本
- `CMAKE_Fortran_COMPILER`, Fortran编译器可执行文件路径
- `CMAKE_Fortran_COMPILER_ID`, Fortran编译器类型/厂商标识
- `CMAKE_Fortran_COMPILER_VERSION`, Fortran编译器版本
  
## 8. set

设置变量的值, 支持普通变量、缓存变量、环境变量

基本语法:

```cmake
set(<变量名> <值>... [PARENT_SCOPE])
```

- <变量名>: 带设置的变量名称, 区分大小写
- <值>: 零个或多个值, 多个值用列表或分号连接

普通变量:

```cmake
set(APP_NAME MyApp)
set(APP_VERSION "1.2.0")

message(STATUS "名称：${APP_NAME}")
message(STATUS "版本：${APP_VERSION}")
```

```powershell
-- 名称：MyApp
-- 版本：1.2.0
```

缓存变量:
(待补充)

环境变量:

```cmake
set(ENV{MY_APP_MODE} "dev")

message(STATUS "$ENV{MY_APP_MODE}")  # dev
```

修改当前CMake进程的环境, 不会永久修改系统环境, 也不会修改启动CMake的终端环境

## 9. set_target_properties

用于设置一个或多个构建目标的属性

基本语法:

```cmake
set_target_properties(
    <目标1> [<目标2> ...]
    PROPERTIES
        <属性名1> <属性值1>
        <属性名2> <属性值2>
)
```

常见写法:

```cmake
add_executable(solver src/main.cpp)  # CMake目标名: solver
set_target_properties(solver PROPERTIES
    OUTPUT_NAME "my_solver"                             # 修改输出文件名: my_solver.exe
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"  # 可执行文件、.dll输出目录
)
```

## 10. target_compile_options

## 11. target_include_directories

用于设置某个目标的头文件或模块文件搜索目录, C/C++查找.h/.hpp, Fortran查找编译器生成的.mod

基本语法:

```cmake
target_include_directories(
    <目标名>
    [SYSTEM]
    [BEFORE]
    <PRIVATE | PUBLIC | INTERFACE>
        <目录>...
)
```

常见写法:

```cmake
add_executable(solver src/main.f90)

target_include_directories(solver PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)
```

## 12. target_link_libraries

## 13. target_sources


