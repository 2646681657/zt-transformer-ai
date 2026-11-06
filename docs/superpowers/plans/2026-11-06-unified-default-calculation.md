# Unified Default Calculation Implementation Plan

> **For agentic workers:** use subagent-driven-development for UI/export and local core integration.

**Goal:** 所有方案统一使用内置SB20默认公式；删除为复核第一组、第三组新增的差异覆盖入口。
**Architecture:** 默认规则改为不可变常数；历史JSON忽略废弃字段，新JSON不再保存；引擎删除替代公式分支。保留真实设计输入、材料价格、20级轭阶梯、已修复通用算法。
**Tech Stack:** C++17, Qt6, PowerShell Excel COM, CMake/CTest.
**Spec:** 用户2026-11-06批准的18类清单，以上一轮统计为范围。

## Global Constraints
- 默认片宽偏移和补充片数+2/-1属于公式，保持固定，不能全部归零。
- 默认铁损继续曲线计算，不以复核单手填铁损替代。
- 已有真实尺寸、油道、杂散系数、工艺系数、接缝数和20级轭阶梯继续可输入。
- 寻优预算上限10000000；不修改寻优策略。
- 历史方案载入后废弃字段不生效；结果缓存版本升级，重新计算。

## Tasks
- [x] 1. tests/unified_default_engine_test.cpp：使用历史JSON污染所有废弃字段，与清除字段后的输入比较结果；原默认固定值回归；正常尺寸改变仍改变结果。先运行失败再实现。
- [x] 2. CalcInput/SchemeStore/Engine：不可变规则、移除旧字段导入导出和替代公式；旧快照失效。
- [x] 3. ParamTableWidget/SchemeCalculationSheet：移除18类可调入口和旧模式提示，保留默认规则计算说明。
- [x] 4. Excel脚本：所有导出统一默认公式，移除复核参数引用，更新导出回归。
- [x] 5. 预置样本及旧测试：移除废弃字段，改为同一规则的不同输入样本，覆盖启动库持久化及界面。
- [x] 6. 编译、针对性CTest、实际Excel重算、自审；备份替换桌面程序；提交PR到dev并合并。
