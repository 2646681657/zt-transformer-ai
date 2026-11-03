# 弹窗恢复与工作范围纠正

2026-10-06：用户反馈应先改设计输入，拒绝a3f733e的弹窗明细扩展。

- EnterCalcPage.cpp和SchemeCalculationSheet.cpp恢复到392b86d的内容；仅撤回上一批UI，保留全屏、三分区紧凑网格、自动计算、确认/取消及输入联动。
- 计算引擎公式及支持范围不变。a3f733e新增的只读结果快照保留，但不在弹窗展示。历史提交、预览exe、快捷方式备份保留，没有重置或删除用户数据。
- 设计输入页本轮未改。下一批从选型与相关输入的真实联动开始，不将长结果清单搬到设计输入页，不再次扩大弹窗布局。
- 人工检查：关闭旧程序后从桌面重新打开，进入方案弹窗，检查上一批新增的基础电量及16级叠积明细已不再插入，原三分区布局和自动计算仍在；取消不保存，确认仍收集最新值。
- 仅Release编译、静态审阅和文件校验，不运行自动化测试或GUI。尚待用户检查，不推送、不创建PR。

桌面目标：`D:/zt-transformer/zt-transformer-ai-main/build/ZTBLD-Designer-scheme-restored-preview.exe`。恢复版SHA256：`E98EAFA884999219148F424F00A41BDC12EC2E97EF48A74B3BE9A5221F2AFE61`，与本次编译文件一致。原快捷方式备份为`build/ZTBLD-before-popup-restore.lnk`。
