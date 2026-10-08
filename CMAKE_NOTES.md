<!-- omit from toc -->
# CMAKE_NOTES
- [1. add\_executable](#1-add_executable)
- [2. cmake\_minimum\_required](#2-cmake_minimum_required)
- [3. foreach](#3-foreach)
- [4. message](#4-message)
- [5. project](#5-project)
- [6. set\_target\_properties](#6-set_target_properties)

## 1. add_executable

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


## 6. set_target_properties