# 用测试自己的main构建独立的可执行文件
add_executable(C3D8Smoke
    C3D8Smoke.cpp
    ../src/C3D8Element.cpp
    ../src/LinearElastic.cpp
)

# 链接公共矩阵库, 继承公开的include路径
target_link_libraries(C3D8Smoke PRIVATE CAEMath)

# 没有Qt类, 关闭AUTOMOC
set_target_properties(C3D8Smoke PROPERTIES AUTOMOC OFF)
