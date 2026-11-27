# 增量材料非线性静力分析

当前支持小应变 C3D8（沿用 B-bar）、线弹性和 J2 各向同性硬化塑性。
塑性曲线是屈服应力与累计等效塑性应变的分段线性关系，首点塑性应变必须为零，
应力不下降；曲线末点之后保持末点屈服应力不变。剪应变使用工程剪应变约定。

## 状态所有权与续算

`AnalysisContext` 表示一次分析任务，持有位移、当前载荷、约束值、伪时间和
`ElementStates`。后者按值保存单元定义，单元与材料通过 `shared_ptr` 保持有效，
因此不依赖构建器或临时问题对象的生命周期。不使用进程级全局变量。

```cpp
const auto problem = builder.buildNonlinearStaticProblem();
AnalysisContext context(problem);
NonlinearStaticSolver solver;
NonlinearStaticOptions options;
options.initialIncrement = 0.1;
auto loading = solver.solve(context, problem.loads, problem.prescribedDisplacements, options);
auto unloading = solver.solve(context, linalg::VectorX(problem.loads.size(), 0.0),
                              problem.prescribedDisplacements, options);
```

每次调用从 context 的已收敛载荷/位移约束线性变化到本次目标。
`duration` 为本次历程的伪时间长度，增量参数为本次历程的比例。
支持不改变约束自由度集合的非零指定位移加载与卸载。
不同分析任务应分别创建 context，重新创建 context 表示从零开始分析。

## Newton 与材料契约

每次迭代从同一 committed 状态计算 trial，材料积分必须填写应力、应变、内变量和
算法一致切线 `MaterialPointState::tangent`。单元使用同一套 B-bar 积分内力及切线。
仅检查自由自由度的力残差，采用绝对与相对容差；约束自由度由指定位移直接消元。
收敛后才提交全部积分点状态。迭代次数耗尽时回滚 trial 并将增量减半，
低于最小增量则抛出异常，context 保留此前最近一个收敛增量。
非法输入、材料异常和非正定切线引发异常时回滚当前 trial 并直接报告错误。

当前使用稠密矩阵与 Cholesky，要求约束后的切线正定；不支持软化、弧长控制、
几何非线性或显式塑性。API 可连续执行多个加载历程；INP 读取仍只支持一个分析步。
内存 context 支持续算，结果 CSV 仅用于后处理，尚不包含重启动所需的塑性内变量。

## 界面与验证

静力 INP 含 `*PLASTIC` 时自动使用增量非线性路径，默认十个加载增量，
未收敛时自动减步长；增量设置目前通过 C++ options 配置。
结果仍使用现有位移、积分点与节点 CSV 格式，并可播放已收敛帧。
`abaqus/plastic-cube/cube.inp` 是单位立方体拉伸算例。

```text
cmake --build build --config Debug -- /nodeReuse:false
ctest --test-dir build -C Debug --output-on-failure
```

`NonlinearStaticSmoke` 校验混合应变下的一致切线、塑性返回映射、
单轴加载解析解、卸载残余应变、反力平衡、上下文生命周期、失败回滚及自动减步长。
