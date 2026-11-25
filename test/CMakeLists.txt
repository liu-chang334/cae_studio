# 用测试自己的main构建独立的可执行文件
add_executable(C3D8Smoke
    C3D8Smoke.cpp
    ../solver/elements/src/C3D8Element.cpp
    ../solver/materials/src/LinearElastic.cpp
)
add_executable(AnalysisModelSmoke
    AnalysisModelSmoke.cpp
    ../io/src/AbaqusInpReader.cpp
    ../solver/analysis/src/AnalysisProblemBuilder.cpp
    ../solver/analysis/src/ElementStates.cpp
    ../solver/analysis/src/LinearStaticSolver.cpp
    ../solver/elements/src/C3D8Element.cpp
    ../solver/materials/src/LinearElastic.cpp
)
# 单元测试需要网格、材料和单元的头文件。
target_include_directories(C3D8Smoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/elements/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/materials/include"
)

# 模型测试还需要 INP 读取器和分析模型的头文件。
target_include_directories(AnalysisModelSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../io/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/analysis/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/elements/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/materials/include"
)

# 链接公共矩阵库, 继承公开的include路径
target_link_libraries(C3D8Smoke PRIVATE CAEMath)
target_link_libraries(AnalysisModelSmoke PRIVATE CAEMath)

# 没有Qt类, 关闭AUTOMOC
set_target_properties(C3D8Smoke PROPERTIES AUTOMOC OFF)
set_target_properties(AnalysisModelSmoke PROPERTIES AUTOMOC OFF)

add_executable(ExplicitDynamicsSmoke
    ExplicitDynamicsSmoke.cpp
    ../io/src/AbaqusInpReader.cpp
    ../solver/analysis/src/AnalysisProblemBuilder.cpp
    ../solver/analysis/src/ElementStates.cpp
    ../solver/analysis/src/ExplicitDynamicsSolver.cpp
    ../solver/analysis/src/LinearStaticSolver.cpp
    ../solver/elements/src/C3D8Element.cpp
    ../solver/materials/src/LinearElastic.cpp
)
target_include_directories(ExplicitDynamicsSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../io/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/analysis/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/elements/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/materials/include"
)
target_link_libraries(ExplicitDynamicsSmoke PRIVATE CAEMath)
set_target_properties(ExplicitDynamicsSmoke PROPERTIES AUTOMOC OFF)

add_executable(DisplacementResultSmoke
    DisplacementResultSmoke.cpp
    ../io/src/DisplacementResultIO.cpp
    ../io/src/ElasticResultIO.cpp
    ../visualization/src/vtkBuilder.cpp
    ../visualization/src/vtkScene.cpp
    ../solver/analysis/src/LinearStaticSolver.cpp
    ../solver/analysis/src/AnalysisProblemBuilder.cpp
    ../solver/analysis/src/ElementStates.cpp
    ../solver/elements/src/C3D8Element.cpp
    ../solver/materials/src/LinearElastic.cpp
)
target_include_directories(DisplacementResultSmoke PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../io/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../model/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/analysis/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../visualization/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/elements/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../solver/materials/include"
)
target_link_libraries(DisplacementResultSmoke PRIVATE CAEMath Qt6::Core VTK::CommonDataModel VTK::FiltersCore VTK::RenderingAnnotation)
set_target_properties(DisplacementResultSmoke PROPERTIES AUTOMOC OFF)
