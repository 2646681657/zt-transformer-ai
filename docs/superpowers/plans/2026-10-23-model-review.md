# 第三批实施计划：独立测试与展示模型审核

依据：../specs/2026-10-23-model-review-design.md（已获用户确认）。

边界：不接入寻优，不改原公式、CalcResult、报价、打印或合格判定。无企业门槛默认拒绝；测试模型永不启用。复用现有独立工作区；所有既有改动保留，验收前不提交。

## 1. Python独立比较（测试先行）

在learning/trainer.py新增evaluate命令：--bundle、--models（一个或多个model.json）、--output新目录、可选--test-only、--cancel-file。读取冻结TrainingBundle；验证所有训练和验证产品分组；固定模型执行预测，不更新模型。输出evaluation.json，逐模型逐目标统计完整标签分母、成功覆盖、失败、原公式/DNN的MAE、RMSE、最大误差及独立产品数；零误差不生成除零改善率。合成标识传播。记录文件SHA256，不能静默丢弃失败样本。

约定报告根：format=ZTBLD-IndependentEvaluation，version=1，status=completed，testOnly，bundlePath，bundleSha256，models数组。模型条目：modelPath、modelSha256、reportPath、reportSha256、modelVersion、scope、engineVersion、testOnly、eligible（仅证据完整无泄漏，不代表获准）、failures数组、targets对象。目标：labelledSamples、independentProducts、predictedSamples、coverage（0..1）、failures数组、baseline/model对象（MAE、RMSE、maxAbsoluteError；没有预测为null）。

## 2. C++审核存储（测试先行）

新增core/LearningModelReview.h/.cpp和tests/model_review_test.cpp。独立registry.json快照含策略、审核和事件/默认版本，使用QLockFile与QSaveFile原子事务；失败不得改状态。审核绑定全部产物、测试包/报告、策略SHA256及范围；每次启用、默认预测和回退重新检查。策略必填来源、版本、confirmed及按目标明确有限非负门槛与正整数最少产品数。默认无策略。测试/损坏/覆盖不足/泄漏/策略变更/审核缺失均阻断。历史回退只选当前仍有效审批。

## 3. 界面集成（测试先行）

在现有LearningTrainingPanel增加冻结测试样本选择、同批模型比较、结果表、策略编辑、审核/默认展示/停用/回退与清晰原因。复用QProcess生命周期与取消保护；测试运行独立保存。默认模型预测与临时实验预测区别显示；切换/停用/错误清除旧结果和tooltip，异步结果核对模型摘要/当前默认。

## 4. 验证和交付

运行现有5项C++回归及新增审核测试、Python单元/真实CPU集成；独立审查本批文件与接口。修复重要问题，再编译桌面EXE。用户关闭后备份原EXE、部署后端与新版、核对SHA256，提供验收点。无真实数据/策略时仅证明流程及阻断，不声称生产精度。

## 进度

- 已检查规格与边界；后端、存储分别拥有独立文件，界面只消费上述报告/审核API。
- 既有基线5项通过（27.78秒）；新增界面测试已观察失败后实现。
- Python独立比较完成，原17项测试通过，正在补充非测试标识的机械夹具门禁测试与运行中取消测试；所有夹具仅在临时目录，不是企业精度证据。
- 界面真实CPU链路通过；审查发现实验预测回填旧值、多窗口策略显示不一致，已观察3项失败并修复后通过。审核保存增加事务内策略摘要CAS核对。
- 整套程序已编译；6项C++回归通过（30.78秒）。新增界面截图已检查，审核门槛保持空白且未保存策略时操作禁用。
- 审查补充已完成：策略P1→P2→P1旧审核永久失效；保存相同策略不撤销；重新审核可启用。新增小数门槛精度失败回归，改用max_digits10后通过，UI和核心均已复审无重要遗留。
- 最终编译及6项C++回归通过（28.62秒）；主进程独立重跑Python21项全部通过（111.104秒）；git diff --check通过。
- 已核对桌面进程关闭、旧版SHA256；保留旧EXE与后端备份，替换相同快捷方式目标，部署后端probe通过，新EXE SHA256与构建产物一致。详细部署记录见docs/MODEL_REVIEW_ACCEPTANCE.md。源代码验收前保持未提交。
