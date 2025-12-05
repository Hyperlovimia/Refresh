<!-- OPENSPEC:START -->
# OpenSpec Instructions

These instructions are for AI assistants working in this project.

Always open `@/openspec/AGENTS.md` when the request:
- Mentions planning or proposals (words like proposal, spec, change, plan)
- Introduces new capabilities, breaking changes, architecture shifts, or big performance/security work
- Sounds ambiguous and you need the authoritative spec before coding

Use `@/openspec/AGENTS.md` to learn:
- How to create and apply change proposals
- Spec format and conventions
- Project structure and guidelines

Keep this managed block so 'openspec update' can refresh the instructions.

<!-- OPENSPEC:END -->

# CLAUDE.md

## 概览

This is an ESP32 embedded systems project built with the Espressif IoT Development Framework (ESP-IDF 5.5.1) using CMake.

实现模块前优先使用 exa MCP 查询官方文档（WiFi / MQTT 等）

所有文档与注释必须使用 **简体中文，UTF-8（无 BOM）**。

明确职责划分：你作为主AI，负责规划与执行。所有分析与审查工作由用户调用 Codex 完成。

### 执行规则

用户表达模糊时主AI必须用多轮提问澄清，可质疑思路并提出更优解。

## Project Structure

```
Refresh/
├── main/                      # Main application component
│   ├── main.c                 # Entry point (app_main function)
│   ├── main.h
│   ├── CMakeLists.txt         # Component configuration
│   ├── actuators/             # Actuator control modules
│   │   ├── fan_control.c
│   │   └── fan_control.h
│   ├── algorithm/             # Decision algorithms
│   │   ├── decision_engine.c
│   │   ├── decision_engine.h
│   │   ├── local_mode.c
│   │   └── local_mode.h
│   ├── network/               # Network communication
│   │   ├── mqtt_client.c
│   │   ├── mqtt_client.h
│   │   ├── weather_api.c
│   │   ├── weather_api.h
│   │   ├── wifi_manager.c
│   │   └── wifi_manager.h
│   ├── sensors/               # Sensor interfaces
│   │   ├── co2_sensor.c
│   │   ├── co2_sensor.h
│   │   ├── sensor_manager.c
│   │   ├── sensor_manager.h
│   │   ├── sht35.c
│   │   └── sht35.h
│   └── ui/                    # User interface
│       ├── oled_display.c
│       └── oled_display.h
├── openspec/                  # OpenSpec documentation and specs
│   ├── AGENTS.md
│   ├── project.md
│   ├── changes/
│   │   └── archive/
│   │   
│   └── specs/
│       ├── actuator-control/
│       ├── decision-algorithm/
│       ├── network-services/
│       ├── sensor-integration/
│       ├── system-orchestration/
│       └── user-interface/
├── test/                      # Test files and documentation
│   ├── JX_CO2_102手册.md
│   └── tmp.md
├── build/                     # Build artifacts (git-ignored)
├── CMakeLists.txt             # Project-level CMake configuration
├── sdkconfig                  # ESP-IDF configuration (git-ignored)
├── sdkconfig.old              # Previous ESP-IDF configuration (git-ignored)
├── README.md
├── AGENTS.md
├── CLAUDE.md
└── .devcontainer/             # Dev container setup for QEMU
```

## 强制验证机制

- 必须拒绝一切 CI、远程流水线或人工外包验证，所有验证均由本地 AI 自动执行。

## 审查协作规范

- 主AI：给出完整审查清单（关注点、检查项、评分、决策规则）。
- Codex：执行深度分析并输出 .claude/review-report.md。
- 评分规则：
    - ≥90 且建议“通过” → 通过
    - <80 且建议“退回” → 退回
    - 其他 → 主AI审阅后裁决
- 主AI可推翻 Codex，但需记录原因。

## 架构优先级

标准化、复用官方 SDK / 社区成熟方案 > 常规搜索 > 本地资料。
禁止自研重复方案，除非已有方案无法满足需求且获特批。
引入外部能力必须确认兼容并写明复用指引。
旧有自研实现需规划替换或下线。

## 代码质量标准

### 注释

- 中文注释解释意图、约束、行为。
- 禁止写“修改说明式注释”。
- 对复杂依赖必须写明设计理由。

### 测试

- 建议提供可自动运行的单元测试或等效验证脚本，由本地 AI 执行。
- 缺失测试需记录风险与补测计划。
- 测试覆盖正常流程、边界条件与错误恢复。

### 设计原则

- 遵循 SOLID、DRY、关注点分离。
- 依赖倒置、接口隔离优先。
- 复杂逻辑先拆分职责。

### 实现标准

- 禁止占位符或未完成实现。
- 必须删除过时代码。
- 破坏性改动无需兼容，但需给出迁移或回滚方案。

### 性能意识

- 关注时间、内存、I/O。
- 避免昂贵依赖与阻塞操作。
- 提供瓶颈监测与优化建议。

### 搜索工具优先级

- 外部检索：优先使用 exa MCP；
- 内部检索：优先使用 code-index；
- 引用资料必须写明来源与用途，保持可追溯。


## 开发流程（6 步）

- 分析需求
- 获取上下文
- 选择工具
- 执行任务
- 验证质量
- 存储知识

## 开发哲学

- 渐进式、小步交付、每次可编译可验证。
- 实现前必须先理解现有代码。
- 简单优先、拒绝炫技。
- 风格、命名、格式必须与现有保持一致。
- 有额外解释代表过于复杂，应继续简化。

### 简单性定义

- 每个函数或类建议仅承担单一责任
- 禁止过早抽象；重复出现三次以上再考虑通用化
- 禁止使用"聪明"技巧，以可读性为先
- 如果需要额外解释，说明实现仍然过于复杂，应继续简化

## 重要约束

### 必须：

- 决策基于证据（现有代码与文档）。
- 对跨模块或超过 5 个子任务的工作进行任务分解与规划。
- 保持计划文档与进度同步更新。

### 禁止：

- 无证据的猜测。
- 上下文写入错误路径（必须为 ./.claude/）。


## 内容唯一性规则

- 必须引用其他层的资料而非复制粘贴，保持信息唯一来源。
- 各层级保持抽象边界清晰。
- 禁止在高层文档中堆叠实现细节，确保架构与实现边界清晰。
- The `build/` directory is git-ignored and contains build artifacts
- The `sdkconfig` and `sdkconfig.old` files are git-ignored; they contain device-specific configurations

## Common Commands

**Environment Prerequsite**
```bash
get_idf  # 如果 build 时提示 "idf.py: Command Not Found"，执行 get_idf 命令
```

**Configure project for target device**:
```bash
idf.py set-target esp32s3  # 默认已经设置好，无需再次设置
```

**Build the project**:
```bash
idf.py build
```

**Full rebuild (clean + build)**:
```bash
idf.py fullclean build
```

**Flash firmware to device**:
```bash
idf.py -p /dev/ttyACM0 flash  # flash 任务执行前，需要提示用户是否执行
```

**Monitor serial output**:
```bash
idf.py -p /dev/ttyACM0 monitor
```

**Combined build and flash**:
```bash
idf.py -p /dev/ttyUSB0 build flash monitor
```

**Menuconfig (configure via GUI)**:
```bash
idf.py menuconfig
```

## References

[JX_CO2_102手册](tmps/JX_CO2_102手册.md)

## Code Examples

Access the local path ($HOME/esp/v5.5.1/esp-idf/examples/)
