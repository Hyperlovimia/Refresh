# Proposal: remove-local-online-decision

## 概述

移除本地联网模式下的决策逻辑，改为通过 MQTT 接收远程服务器的风扇控制命令。

## 背景

当前系统在联网模式（MODE_NORMAL/MODE_DEGRADED）下：
- 本地调用天气 API 获取室外数据
- 本地使用 Benefit-Cost 算法综合传感器和天气数据决策风扇启停

目标架构：
- 远程服务器负责调用天气 API 和决策算法
- 本地设备只负责：
  1. 通过 MQTT 上报传感器数据
  2. 接收并执行远程风扇控制命令
  3. 离线时使用本地模式（MODE_LOCAL）自主决策

## 变更范围

### 删除
- `decision_engine.c` 中 MODE_NORMAL/MODE_DEGRADED 的 Benefit-Cost 决策逻辑
- `network_task` 中天气 API 调用逻辑
- `weather_api` 模块

### 新增
- MQTT 订阅 `home/ventilation/command` 主题
- 远程命令解析和执行逻辑
- MODE_REMOTE 系统模式（联网时由远程控制）

### 修改
- `decision_engine` 在联网模式下直接返回远程命令指定的风扇状态
- `decision_detect_mode` 逻辑简化（不再依赖天气缓存）

## 约束

- 传感器数据必须为有效状态才上报
- 离线模式（MODE_LOCAL）保持不变，仍使用 CO2 阈值决策
- 安全停机模式（MODE_SAFE_STOP）保持不变

## 状态

- [ ] 待审批
