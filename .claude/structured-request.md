# Structured Request — expand-multi-fan-control 审查
日期：2025-12-07 17:50 (UTC+8)  
请求人：用户（通过主AI转交）

## 背景
- 现有系统仅支持 1 个风扇；网络/MQTT 协议中只有 `fan_state` 单字段。
- 变更 `expand-multi-fan-control` 要求扩展为 3 个独立风扇，并允许远程控制端分别开关/调速。

## 目标
1. **硬件控制**：`fan_control` 模块需提供 `FanId` 维度的 PWM 控制，GPIO 映射为 Fan0→GPIO26、Fan1→GPIO27、Fan2→GPIO33，全部运行在 LEDC TIMER0 上（参见 `openspec/changes/expand-multi-fan-control/specs/actuator-control/spec.md`）。
2. **协议扩展**：MQTT 状态 JSON 与命令 JSON 均改为 `fan_0/fan_1/fan_2` 字段，决策/主任务要维护 `FanState[FAN_COUNT]` 缓冲，确保远程命令支持部分更新（参见 `openspec/changes/expand-multi-fan-control/specs/network-services/spec.md`）。

## 输入资料
- 代码：`main/main.h`, `main/actuators/fan_control.*`, `main/algorithm/decision_engine.*`, `main/network/mqtt_wrapper.*`, `main/main.c`。
- 规格：`openspec/changes/expand-multi-fan-control/specs/*`、对应 `tasks.md` 与 `design.md`。
- 运行结果：编译输出 `[17/17] ... Project build complete`（用户反馈）。

## 交付预期
- 确认上述代码/接口是否满足三风扇与多路 MQTT 控制需求。
- 输出审查结论（评分 + 主要问题），并将发现记入 `.claude/review-report.md`。
- 若发现风险/未完成项，需提出修复建议或后续步骤。
