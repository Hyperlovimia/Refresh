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

## 工作原则

- **三方协作**: 你作为主AI，只负责规划与执行。`codex` MCP 负责审查逻辑/定位 bug，`gemini` MCP 负责设计前端 UI。
- **交流语言**: 文档与注释必须使用 **简体中文，UTF-8（无 BOM）** 编写。与用户中文交流，与 Codex/Gemini 英文。
- **明确需求**: 用户表达模糊时，主AI必须用多轮提问澄清，可质疑思路并提出更优解。
- **语义理解**: 
    - 外部检索：优先使用 `exa` MCP；
    - 内部检索：优先使用 `code-index` MCP；
    - 引用资料必须写明来源与用途，保持可追溯。
- **深度思考**: 复杂任务规划、复杂逻辑设计、大幅修改代码等所有复杂工作，调用 `sequential-thinking` MCP。

## 三方协作流程

需求分析 → 告知 codex/gemini 原始需求 + 初始思路 → 迭代讨论
↓
编码前 → 向 codex(后端) 或 gemini(前端) 索要代码原型
↓
实施 → 以原型为参考重写为生产级代码
↓
完成后 → codex review 代码改动

**注意**: codex/gemini 仅供参考，保持独立思考和质疑。

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
│   │   ├── mqtt_wrapper.c
│   │   ├── mqtt_wrapper.h
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
│
├── Guides/                    # User guides and device manuals
│   ├── JX_CO2_102手册.md
│   └── MQTT.md
├── build/                     # Build artifacts (git-ignored)
├── CMakeLists.txt             # Project-level CMake configuration
├── sdkconfig                  # ESP-IDF configuration (git-ignored)
├── sdkconfig.old              # Previous ESP-IDF configuration (git-ignored)
├── README.md
├── .devcontainer/             # Dev container setup for QEMU
└── .claude/
    ├── AGENTS.md              # Codex core principles
    ├── CLAUDE.md              # Claude core principles
    ├── commands/openspec/
    └── docs/
        ├── mcp-*.md        # MCP tool references
        └── ...

```

## 文档索引

| 主题 | 路径 | 说明 |
|------|------|------|
| Codex | `.claude/docs/mcp-codex.md` | 后端/逻辑/Debug |
| Gemini | `.claude/docs/mcp-gemini.md` | 前端/UI/规划 |

## 架构优先级

标准化、复用官方 SDK / 社区成熟方案 > 常规搜索 > 本地资料。
禁止自研重复方案，除非已有方案无法满足需求且获特批。
引入外部能力必须确认兼容并写明复用指引。
旧有自研实现需规划替换或下线。

## 代码质量标准

### 注释要求

- **简体中文，UTF-8（无 BOM）**
- 解释意图、约束、行为。
- 禁止写“修改说明式注释”。
- 对复杂依赖必须写明设计理由。

### 设计原则

- 遵循 SOLID、DRY、关注点分离。
- 依赖倒置、接口隔离优先。

### 实现标准

- 禁止占位符或未完成实现（除非用户要求）。
- 必须删除过时代码。
- 破坏性改动无需兼容，但需给出迁移或回滚方案。
- 拒绝一切 CI、远程流水线或人工外包验证，所有验证均由本地 AI 自动执行。

## 开发哲学

- 渐进式、小步交付、每次可编译可验证。
- 简单优先、拒绝炫技。
- 风格、命名、格式必须与现有保持一致。
- 有额外解释代表过于复杂，应继续简化。

### 简单性定义

- 每个函数或类建议仅承担单一责任
- 禁止过早抽象；重复出现三次以上再考虑通用化
- 禁止使用"聪明"技巧，以可读性为先
- 如果需要额外解释，说明实现仍然过于复杂，应继续简化

## Common Commands

**Environment Prerequsite**
```bash
get_idf  # 当 build 时提示 "idf.py: Command Not Found"，则需要执行 get_idf 命令
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
