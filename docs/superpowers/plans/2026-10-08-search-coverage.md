# Search Coverage Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans. Steps use checkbox syntax for tracking.

**Goal:** 一次交付预算、中心多样性、受控扩展、主油道与企业约束四项。
**Architecture:** 冻结设置与输入共享统一坐标、去重和预算；中心池、范围规划、工艺模型独立于物理公式。增强候选按阶段替换交付，旧模式逐条交付保持不变。
**Tech Stack:** C++17 / Qt6 / MinGW Release。
**Spec:** docs/superpowers/specs/2026-10-08-search-coverage-design.md

## Global Constraints

- 默认预算100000；可选1000..1000000；增强、扩展、主油道参与及企业约束首次关闭。
- 增强保留最低成本5000个可行方案，成本并列按评估顺序；独立中心池每牌号最多256，中心3..12默认6。
- 细搜轮数1..5，扩展轮数1..5；不增加牌号和圆线清单，不放宽性能，不推测其他形状。
- 不创建或运行测试，不启动GUI；仅Release编译、静态审查、用户验收。验收前不push/PR。
- 已批准四项一起交付；复用现有codex隔离分支，不覆盖原工作目录修改。

## Task 1: 搜索模型、策略与工作线程

**Files:** Modify qt-app/src/engine/{IOptimizer.h,GridSearchSpace.h,GridOptimizer.cpp}, qt-app/src/core/{CalcInput.h,SchemeStore.h}; Create qt-app/src/engine/{SearchCenterPool.h,SearchCoveragePlanner.h}, qt-app/src/core/CraftConstraints.h. Engine入口仅增加工艺验证，不改物理公式。

**Interfaces:** Settings: `int combinationBudget=100000; bool enhancedCoverage=false; int centerLimit=6; bool expandCoverage=false; int maxExpansionRounds=2; bool searchMainDuct=false; double mainDuctStep_mm=0.5; int mainDuctRange=1; std::array<QString,9> hardMinimumTexts{},hardMaximumTexts{}; CraftConstraints craftConstraints;`。
数值边界数组顺序：直径、直线段、低压匝数、高压层数、箔厚、箔宽、裸线宽、裸线厚、主油道；空双端不扩展，单端空或无效拒绝。
CraftConstraints: `bool enabled=false; double minimumMainDuct_mm=0; QString source; QString validationError() const; QString rejectionReason(double width) const; QString description() const;`。CalcInput追加同名成员。启用需有限正最小值和非空来源；旧JSON缺对象默认未校核，显式损坏不能静默关闭。
Stage追加`int craftRejected=0`且processed包含两个预检；Run追加`int retained=0; int omitted=0; QVector<OptimizationRefinementSummary> expansions; QStringList centerDetails,coverageDetails;`，原计数保留真实可行数。
IOptimizer追加`void candidatesReady(const QVector<OptimizeCandidate>& candidates)`，增强每阶段替换，不与candidateReady重复发送。

- [x] 设置及合法性：饱和乘积避免第9维溢出，旧模式保守上界验证，增强粗搜<=预算。主油道步长0.1..5且所有生成值有限正数；硬边界涵盖初始范围，整数合法且无坐标溢出。
```cpp
if (processed >= settings.combinationBudget) { budgetExhausted=true; return false; }
// 只有唯一实际处理点耗预算；人工stop独立于budgetExhausted。
```
- [x] 中心池：全局最好必选，先按轮转覆盖牌号代表，再以粗步归一化距离选择不同区域；每牌号成本有序256点，记录淘汰和未获中心名单。
- [x] 规划器：固定基准与尺度，硬边界格点裁剪；各触边方向至多向外一粗步，其他维度局部邻域；各牌号/方向交错处理，每轮扩展后重新局部细化。低改善不能取消仍可扩展方向。
- [x] 工作线程：预算包括线型/工艺预检；全阶段去重；阶段计时/计划/实际/未处理一致；暂停停止可中断生成与计算。增强按成本及顺序保留5000，阶段批量交付；进度百分比变化才发送。
- [x] 保存、加载和重新计算工艺快照，未启用显示未校核；不伪造企业库存表。
- [x] 静态审查本任务全部差异，记录实现报告；不运行程序或测试。

## Task 2: 配置界面、候选交付、统计导出

**Files:** Modify qt-app/src/gui/pages/EnterCalcPage.{cpp,h}; Create qt-app/src/gui/SearchCoverageSettings.h（独立配置子对话框）；其余布局保持。
**Interfaces:** 消费Task1字段，`onOptimizeCandidates(const QVector<OptimizeCandidate>&)`负责阶段替换候选及映射；原候选slot不变。

- [x] 循环参数增加预算1000..1000000；增强勾选和高级设置子对话框，中心3..12、扩展开关/轮数、九维绝对硬边界空文本保留、工艺启用/最小值/来源。取消不保存，确认经完整validationError。
```cpp
settings.setValue("optimize/combinationBudget", s.combinationBudget);
settings.setValue("optimize/enhancedCoverage", s.enhancedCoverage);
// 其余新增字段逐项持久化，硬边界保留原文本，不从空值推断安全限值。
```
- [x] 主油道作为第10行（圆线仍第9行），独立参与/步长/范围；实际上下界、计划实时预览。
- [x] 增强阶段候选替换期间禁用表更新/排序，清空旧映射与标记，重新编号完整候选；运行中禁止确认或修改阶段快照。
- [x] 统计显示实际可行/保留/未保留、预算消耗、中心池裁剪、各牌号中心与未覆盖、各扩展阶段、初始/实际探索/硬边界及停止原因；探索包围盒不等于穷举。报告与显示同一文本。
- [x] 工艺淘汰独立于计算失败/性能；保存重算继续相同条件。
- [x] 静态交叉核对字段名、旧模式文案、数据快照及计数恒等式。

## Task 3: 集成、审查、桌面交付

**Files:** 当前改动与本计划；独立预览及快捷方式备份位于原build目录。
- [x] 请求只读代码审查预算、边界、中心、计数、序列化、Qt queued顺序；修复发现后重新审查。
- [x] `cmake --build D:/zt-transformer/zt-transformer-ai-main/build/codex-shape-guard --config Release -j4`，BUILD_TESTING保持OFF。
- [x] 核对旧快捷方式，保存新唯一备份，复制新唯一预览，更新桌面TargetPath；校验SHA256相同，不启动程序。
- [ ] 本地提交；说明改动入口及人工检查项，待验收再上传和PR。

## 执行记录

2026-10-08：设计已批准；用户明确四项一起交付，统一验收。两个实现面共享字段但不共享写文件，策略实现与界面接入可并行；最终整体静态审查。

2026-10-09继续：四项实现均已写入；界面静态复审通过。整版静态审查发现主油道回显精度及空阶段完成标记问题，已修正，修正范围复审的Spec与Quality均通过，未留未处理问题。最终Release链接已成功，继续后的重新构建返回exit 0（ninja: no work to do），BUILD_TESTING=OFF；未创建或运行测试、未启动应用。桌面已部署独立search-coverage-all-preview程序，旧快捷方式备份为build/ZTBLD-before-search-coverage-all.lnk；源产物与预览SHA256均为33428B2795A05B43CC41784E4B7232568371B73840E7E11BB711C8919EA13629。实际效果待用户验收，验收前不上传/PR。
