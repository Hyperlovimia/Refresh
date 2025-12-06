# Tasks: remove-local-online-decision

## 任务列表

### 1. 新增 MQTT 命令订阅功能
- 文件: `main/network/mqtt_wrapper.c`, `main/network/mqtt_wrapper.h`
- 订阅主题: `home/ventilation/command`
- 解析 JSON 命令: `{"fan_state": "OFF|LOW|HIGH"}`
- 提供 `mqtt_get_remote_command()` 接口获取最新命令
- 验证: 编译通过，日志显示订阅成功

### 2. 新增 MODE_REMOTE 系统模式
- 文件: `main/main.h`
- 在 `SystemMode` 枚举中添加 `MODE_REMOTE`
- 验证: 编译通过

### 3. 修改决策引擎
- 文件: `main/algorithm/decision_engine.c`, `main/algorithm/decision_engine.h`
- 删除 MODE_NORMAL/MODE_DEGRADED 的 Benefit-Cost 逻辑
- 新增 `decision_make_remote(FanState remote_cmd)` 或修改 `decision_make` 支持远程命令
- MODE_REMOTE 时直接返回远程命令的风扇状态
- 验证: 编译通过

### 4. 修改模式检测逻辑
- 文件: `main/algorithm/decision_engine.c`
- `decision_detect_mode` 不再依赖 `cache_valid` 参数
- WiFi 连接 → MODE_REMOTE
- WiFi 断开 → MODE_LOCAL
- 传感器故障 → MODE_SAFE_STOP
- 验证: 编译通过

### 5. 修改主程序决策任务
- 文件: `main/main.c`
- `decision_task` 在 MODE_REMOTE 时从 MQTT 获取远程命令
- 验证: 编译通过

### 6. 移除天气 API 调用
- 文件: `main/main.c`
- 删除 `network_task` 中天气数据拉取逻辑
- 删除 `shared_weather_data` 相关代码
- 验证: 编译通过

### 7. 删除天气 API 模块
- 文件: `main/network/weather_api.c`, `main/network/weather_api.h`
- 删除模块
- 更新 `main/CMakeLists.txt`（如需要）
- 验证: 完整编译通过 `idf.py build`

## 依赖关系

```
1 (MQTT订阅) → 3 (决策引擎) → 5 (主程序)
2 (MODE_REMOTE) → 3, 4, 5
4 (模式检测) → 5
6 (移除天气调用) → 7 (删除模块)
```

## 可并行任务

- 任务 1 和 任务 2 可并行
- 任务 6 和 任务 7 可并行
