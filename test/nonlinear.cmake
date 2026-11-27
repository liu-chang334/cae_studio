foreach(target AnalysisModelSmoke ExplicitDynamicsSmoke DisplacementResultSmoke)
    target_sources(${target} PRIVATE ../solver/materials/src/MisesPlasticity.cpp)
endforeach()
add_executable(NonlinearStaticSmoke
    NonlinearStaticSmoke.cpp ../io/src/AbaqusInpReader.cpp
    ../solver/analysis/src/AnalysisProblemBuilder.cpp
    ../solver/analysis/src/NonlinearStaticSolver.cpp
    ../solver/analysis/src/LinearStaticSolver.cpp
    ../solver/analysis/src/ElementStatesManager.cpp
    ../solver/materials/src/LinearElastic.cpp
    ../solver/materials/src/MisesPlasticity.cpp
    ../solver/elements/src/C3D8Element.cpp)
target_include_directories(NonlinearStaticSmoke PRIVATE
    ../io/include ../model/include ../solver/analysis/include
    ../solver/materials/include ../solver/elements/include)
target_link_libraries(NonlinearStaticSmoke PRIVATE CAEMath)
set_target_properties(NonlinearStaticSmoke PROPERTIES AUTOMOC OFF)
add_test(NAME NonlinearStaticSmoke COMMAND NonlinearStaticSmoke
    "${CMAKE_CURRENT_SOURCE_DIR}/../abaqus/plastic-cube/cube.inp")
add_test(NAME C3D8Smoke COMMAND C3D8Smoke)
add_test(NAME AnalysisModelSmoke COMMAND AnalysisModelSmoke
    "${CMAKE_CURRENT_SOURCE_DIR}/../abaqus/job1-static/job1.inp")
add_test(NAME ExplicitDynamicsSmoke COMMAND ExplicitDynamicsSmoke
    "${CMAKE_CURRENT_SOURCE_DIR}/../abaqus/job1-explicit/job1.inp")
add_test(NAME DisplacementResultSmoke COMMAND DisplacementResultSmoke)
