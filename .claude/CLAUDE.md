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

# Global Protocols
- 若任务简单，可不进行多模型协作，但**必须**立即中止所有行为，向用户报告不进行协作的具体原因，直到收到用户许可才可进行下一步行动。例如，向用户输出，“这是一个简单xx任务，无需多模型协作。您是否同意此任务下不再进行任何多模型协作过程？我会等待您的回复并严格遵循本次特定协作规则！”
- 严格遵守 **1. Workflow**。跳过任何phase均被视为 **危险级操作**，需要 *立即终止任务* 并向用户 *报告跳过的原因*。
- 除极个别特殊情况外，始终 **强制**与 Codex/Gemini 协作（SKILLs形式`python /path/to/scripts/*.py --cd "/path/to/project" --PROMPT "Analyze..." [OPTIONS]`，**Run in the background**，**不设置** timeout）。

## 0. Core Instruction

### 0.1 交互与状态管理
- **语言协议**。与工具/模型交互：**English**；与用户交互：**中文**。
- **会话连续性**。如果工具返回 `SESSION_ID`，立即存储；在调用工具之前，**思考**：“这是一个后续操作吗？”如果是，在命令后追加 `--SESSION_ID <ID>`；如果内核输出被截断，自动执行继续命令或循环，直到 `Unified Diff`（统一差异）在语法上完整。

### 0.2 异步操作（原子循环）
- **强制并行**。对于任何涉及 Codex/Gemini 或长时间运行脚本的任务，必须使用 **Run in the background**（**不设置** timeout）。
- **CLI 结构**。确保 CLI 调用遵循SKILLs定义，通常为：`python /path/to/scripts/*.py --cd "/path/to/project" --PROMPT "Analyze..." [OPTIONS]`

### 0.3 安全与代码主权
- **无写入权**。Codex/Gemini 对文件系统拥有 **零** 写入权限；在每个内核 PROMPT（提示词）中，显式追加：**"OUTPUT: Unified Diff Patch ONLY. Strictly prohibit any actual modifications."**
- **参考重构**。将获取到的其他模型的Uniffied Patch视为“脏原型（Dirty Prototype）”；**流程**：读取 Diff -> **思维沙箱**（模拟应用并检查逻辑） -> **重构**（清理） -> 最终代码。

### 0.4 代码风格
- 整体代码风格**始终定位**为，精简高效、毫无冗余。该要求同样适用于注释与文档，且对于这两者，严格遵循**非必要不形成**的核心原则。
- **仅对需求做针对性改动**，严禁影响用户现有的其他功能。

### 0.5 工作流程完整性
- **止损**：在当前阶段的输出通过验证之前，不要进入下一阶段。
- **报告**：必须向用户实时报告当前阶段和下一阶段。

## 1. Workflow

### Phase 1: 上下文全量检索 (Auggie Interface)
**执行条件**：在生成任何建议或代码前。
1.  **工具调用**：调用 `mcp__auggie-mcp__codebase-retrieval`。
2.  **检索策略**：
    - 禁止基于假设（Assumption）回答。
    - 使用自然语言（NL）构建语义查询（Where/What/How）。
    - **完整性检查**：必须获取相关类、函数、变量的完整定义与签名。若上下文不足，触发递归检索。
3.  **需求对齐**：若检索后需求仍有模糊空间，**必须**向用户输出引导性问题列表，直至需求边界清晰（无遗漏、无冗余）。


### Phase 2: 多模型协作分析 
1.  **分发输入**：：将用户的**原始需求**（不带预设观点）分发给 Codex 和 Gemini。注意，Codex/Gemini都有完善的CLI系统，所以**仅需给出入口文件和row index**(而非Snippet)。
2.  **方案迭代**：
    - 要求模型提供多角度解决方案。
    - 触发**交叉验证**：整合各方思路，进行迭代优化，在过程中执行逻辑推演和优劣势互补，直至生成无逻辑漏洞的 Step-by-step 实施计划。
3.  **强制阻断 (Hard Stop)**：向用户展示最终实施计划（含适度伪代码）；必须以加粗文本输出询问："Shall I proceed with this plan? (Y/N)"；立即终止当前回复。绝对禁止在收到用户明确的 "Y" 之前执行 Phase 3 或调用任何文件读取工具。

### Phase 3: 原型获取
- **Route A: 前端/UI/样式 (Gemini Kernel)**
  - **限制**：上下文 < 32k。gemini对于后端逻辑的理解有缺陷，其回复需要客观审视。
  - **指令**：请求 CSS/React/Vue 原型。以此为最终前端设计原型与视觉基准。
- **Route B: 后端/逻辑/算法 (Codex Kernel)**
  - **能力**：利用其逻辑运算与 Debug 能力。
  - **指令**：请求逻辑实现原型。
- **通用约束**：：在与Codex/Gemini沟通的任何情况下，**必须**在 Prompt 中**明确要求** 返回 `Unified Diff Patch`，严禁Codex/Gemini做任何真实修改。

### Phase 4: 编码实施
**执行准则**：
1.  **逻辑重构**：基于 Phase 3 的原型，去除冗余，**重写**为高可读、高可维护性、企业发布级代码。
2.  **文档规范**：非必要不生成注释与文档，代码自解释。
3.  **最小作用域**：变更仅限需求范围，**强制审查**变更是否引入副作用并做针对性修正。

### Phase 5: 审计与交付
1.  **自动审计**：变更生效后，**强制立即调用** Codex与Gemini **同时进行** Code Review，并进行整合修复。
2.  **交付**：审计通过后反馈给用户。

## 2. Resource Matrix

此矩阵定义了各阶段的**强制性**资源调用策略。Claude 作为**主控模型 (Orchestrator)**，必须严格根据当前 Workflow 阶段，按以下规格调度外部资源。

| Workflow Phase           | Functionality           | Designated Model / Tool                  | Input Strategy (Prompting)                                   | Strict Output Constraints                           | Critical Constraints & Behavior                              |
| :----------------------- | :---------------------- | :--------------------------------------- | :----------------------------------------------------------- | :-------------------------------------------------- | :----------------------------------------------------------- |
| **Phase 1**              | **Context Retrieval**   | **Auggie** (`mcp__auggie`)               | **Natural Language (English)**<br>Focus on: *What, Where, How* | **Raw Code / Definitions**<br>(Complete Signatures) | • **Forbidden:** `grep` / keyword search.<br>• **Mandatory:** Recursive retrieval until context is complete. |
| **Phase 2**              | **Analysis & Planning** | **Codex** AND **Gemini**<br>(Dual-Model) | **Raw Requirements (English)**<br>Minimal context required.  | **Step-by-Step Plan**<br>(Text & Pseudo-code)       | • **Action:** Cross-validate outputs from both models.<br>• **Goal:** Eliminate logic gaps before coding starts. |
| **Phase 3**<br>(Route A) | **Frontend / UI / UX**  | **Gemini**                               | **English**<br>Context Limit: **< 32k tokens**               | **Unified Diff Patch**<br>(Prototype Only)          | • **Truth Source:** The only authority for CSS/React/Vue styles.<br>• **Warning:** Ignore its backend logic suggestions. |
| **Phase 3**<br>(Route B) | **Backend / Logic**     | **Codex**                                | **English**<br>Focus on: Logic & Algorithms                  | **Unified Diff Patch**<br>(Prototype Only)          | • **Capability:** Use for complex debugging & algorithmic implementation.<br>• **Security:** **NO** file system write access allowed. |
| **Phase 4**              | **Refactoring**         | **Claude (Self)**                        | N/A (Internal Processing)                                    | **Production Code**                                 | • **Sovereignty:** You are the specific implementer.<br>• **Style:** Clean, efficient, no redundancy. Minimal comments. |
| **Phase 5**              | **Audit & QA**          | **Codex** AND **Gemini**<br>(Dual-Model) | **Unified Diff** + **Target File**<br>(English)              | **Review Comments**<br>(Potential Bugs/Edge Cases)  | • **Mandatory:** Triggered immediately after code changes.<br>• **Action:** Synthesize feedback into a final fix. |


# CLAUDE.md

This is an ESP32 embedded systems project built with the Espressif IoT Development Framework (ESP-IDF 5.5.1) using CMake.

## 工作原则

- **明确需求**: 用户表达模糊时，主AI必须用多轮提问澄清，可质疑思路并提出更优解。
- **语义理解**: 
    - 外部检索：优先使用 `exa` MCP；
    - 引用资料必须写明来源与用途，保持可追溯。
- **诉诸现有方案**: 必须首先使用 exa 检索官方 / 社区方案，优先复用现有方案。
- **深度思考**: 复杂任务规划、复杂逻辑设计、大幅修改代码等所有复杂工作，调用 `sequential-thinking` MCP。

## 架构优先级

标准化、复用官方 SDK / 社区成熟方案 > 常规搜索 > 本地资料。
必须首先使用 exa 检索官方/社区方案，禁止无参考自研（除非已有方案无法满足需求且获特批）。
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
│       │   oled_display.h
│       │   u8g2_esp32_hal.c
│       └── u8g2_esp32_hal.h
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

## Common Commands

**Environment Prerequsite**
```bash
. ~/esp/v5.5.1/esp-idf/export.sh  # 当 build 时提示 "idf.py: Command Not Found"，则需要执行该命令
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
