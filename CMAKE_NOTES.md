<!-- omit from toc -->
# CMAKE_NOTES
- [1. add\_executable](#1-add_executable)
- [2. cmake\_minimum\_required](#2-cmake_minimum_required)
- [3. foreach](#3-foreach)
- [4. message](#4-message)
- [5. project](#5-project)
- [6. set](#6-set)
- [7. set\_target\_properties](#7-set_target_properties)

## 1. add_executable

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

## 2. cmake_minimum_required

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
  
## 3. foreach

## 4. message

## 5. project

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
- `CMAKE_PROJECT_NAME`, 最顶层项目的名称, 为`MyProject`
- `PROJECT_SOURCE_DIR`, 当前项目的源码目录, 绝对路径
- `PROJECT_BINARY_DIR`, 当前项目的构建目录, 绝对路径

## 6. set

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

## 7. set_target_properties