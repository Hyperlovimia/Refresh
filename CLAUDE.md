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

本文件用于指导在当前仓库内进行的全部开发与文档工作，确保输出遵循强制性标准并保持可审计性。

必须明确职责划分：你作为主AI，负责规划与执行工作，而所有分析与审查工作必须请求用户处理，用户将自行使用 Codex 进行分析。

所有文档以及注释必须用简体中文编写。

### 执行规则
当我表达模糊时，你必须用多轮提问澄清。可以质疑我的思路，考虑更优的选择。

## 强制验证机制
- 必须拒绝一切 CI、远程流水线或人工外包验证，所有验证均由本地 AI 自动执行。
- 每次改动必须提供可重复的本地验证步骤（脚本、指令或推理说明），并在失败时立即终止提交。
- 验证过程中如遇工具缺失或测试覆盖不足，必须在任务文档中记录原因和补偿计划。
- 若存在无法验证的部分，必须先补足验证能力或将任务退回，不允许带缺陷交付。

## 🤝 审查协作规范

| instruction | notes |
| --- | --- |
| 主AI职责 - 定义审查清单,明确审查关注点、检查项、评分标准和决策规则 | 一次性传递完整审查要求 |
| Codex职责 - 执行深度审查分析,进行推理,输出评分、建议和论据 | 充分利用Codex推理能力 |
| 主AI请求用户调用Codex执行审查,传递完整审查清单与验收标准 | 需要用户操作 |
| Codex生成 `.claude/review-report.md` 审查报告,包含:元数据、评分详情(技术+战略+综合)、明确建议、核对结果、风险与阻塞、留痕文件、反馈通道 | 结果可追溯 |
| 主AI决策规则:综合评分≥90分且Codex建议"通过"→直接确认通过;综合评分<80分且Codex建议"退回"→直接确认退回;其他情况(80-89分或建议"需讨论")→仔细审阅报告后决策 | 信任明确案例,聚焦边界案例 |
| 主AI保留随时推翻Codex建议的权力,但需记录推翻原因用于优化Codex审查标准 | 最终决策权在主AI |

## 架构优先级
- "标准化 + 生态复用"拥有最高优先级，必须首先使用 exa MCP 查找并复用官方 SDK、社区成熟方案或既有模块。
- 禁止新增或维护自研方案，除非已有实践无法满足需求且获得记录在案的特例批准。
- 在引入外部能力时，必须验证其与项目标准兼容，并编写复用指引。
- 对现有自研或偏离标准的实现，必须规划替换或下线时间表，确保维护成本持续下降。


## ✅ 代码质量强制标准

### 📝 注释规范
- 所有文档与必要代码注释必须使用简体中文，描述意图、约束与使用方式。
- 禁止编写"修改说明"式注释，所有变更信息应由版本控制和日志承担。
- 当模块依赖复杂或行为非显而易见时，必须补充中文注释解释设计理由。

### 🧪 测试规范
- 每次实现必须提供可自动运行的单元测试或等效验证脚本，由本地 AI 执行。
- 缺失测试的情况必须在验证文档中列为风险，并给出补测计划与截止时间。
- 测试需覆盖正常流程、边界条件与错误恢复，确保破坏性变更不会遗漏关键分支。

### 🏗️ 设计原则
- 严格遵循 SOLID、DRY 与关注点分离，任何共享逻辑都应抽象为复用组件。
- 依赖倒置与接口隔离优先，禁止临时绑死实现细节。
- 遇到复杂逻辑时必须先拆分职责，再进入编码。

### 💻 实现标准
- 绝对禁止 MVP、最小实现或占位符；提交前必须完成全量功能与数据路径。
- 必须主动删除过时、重复或逃生式代码，保持实现整洁。
- 对破坏性改动不做向后兼容处理，同时提供迁移步骤或回滚方案。

### ⚡ 性能意识
- 设计时必须评估时间复杂度、内存占用与 I/O 影响，避免无谓消耗。
- 识别潜在瓶颈后应提供监测或优化建议，确保可持续迭代。
- 禁止引入未经评估的昂贵依赖或阻塞操作。

### 🧩 测试思维
- 在编码前编制可验证的验收条件，并在验证文档中回填执行结果。
- 对预期失败场景提供处理策略，保证服务可控降级。
- 连续三次验证失败必须暂停实现，回到需求和设计阶段复盘。

## 搜索工具优先级（必须）
- 外部信息检索必须优先使用 exa MCP；如需其他搜索引擎，需说明原因并补充引用。
- 内部代码或文档检索必须优先使用 code-index；若工具不可用，需在日志中声明并改用人工方法。
- 所有引用资料必须写明来源与用途，保持可追溯。

## Project Structure

```
Refresh/
├── main/                      # Main application component
│   ├── main.c                 # Entry point (app_main function)
│   └── CMakeLists.txt         # Component configuration
├── build/                     # Build artifacts (git-ignored)
├── CMakeLists.txt             # Project-level CMake configuration
├── sdkconfig                  # ESP-IDF configuration (git-ignored)
└── .devcontainer/             # Dev container setup for QEMU
```

## Build and Development

### Prerequisites
- 必须首先在 bash 执行 get_idf ，以初始化环境变量
（参考信息 ~/.bashrc：alias get_idf='. $HOME/esp/v5.5.1/esp-idf/export.sh'）
- CMake (typically included with ESP-IDF)
- idf.py tool (part of ESP-IDF)

#### 路径规范
所有任务执行产生的工作文件必须写入项目本地 `.claude/` 目录（而非全局 `~/.claude/`）：

### 📋 标准工作流 6 步骤（必须执行）
1. 分析需求
2. 获取上下文
3. 选择工具
4. 执行任务
5. 验证质量
6. 存储知识

### 🔄 渐进式上下文收集流程（必须）

#### 核心哲学
- **问题驱动**：基于关键疑问收集，而非机械执行固定流程
- **充分性优先**：追求"足以支撑决策和规划"，而非"信息100%完整"
- **动态调整**：根据实际需要决定深挖次数（建议≤3次），避免过度收集
- **成本意识**：每次深挖都要明确"为什么需要"和"解决什么疑问"

#### 步骤1：结构化快速扫描（必须）
进行框架式收集，输出到 `.claude/context-claude.md`：
- 位置：功能在哪个模块/文件？
- 现状：现在如何实现？找到1-2个相似案例
- 技术栈：使用的框架、语言、关键依赖
- 测试：现有测试文件和验证方式

#### 步骤2：识别关键疑问（必须）
主AI使用 sequential-thinking 分析初步收集和观察报告，识别关键疑问：
- 我理解了什么？（已知）
- 还有哪些疑问影响规划？（未知）
- 这些疑问的优先级如何？（高/中/低）
- 输出：优先级排序的疑问列表

#### 步骤3：针对性深挖（按需，建议≤3次）
仅针对高优先级疑问，请求用户操作，用户将通过 Codex 深挖：
- 聚焦单个疑问，不发散
- 提供代码片段证据，而非猜测
- **成本提醒**：第3次深挖时提醒"评估成本"，第4次及以上警告"建议停止，避免过度收集"

#### 步骤4：充分性检查（必须）
在进入任务规划前，主AI必须回答充分性检查清单：
- □ 我能定义清晰的接口契约吗？（知道输入输出、参数约束、返回值类型）
- □ 我理解关键技术选型的理由吗？（为什么用这个方案？为什么有多种实现？）
- □ 我识别了主要风险点吗？（并发、边界条件、性能瓶颈）
- □ 我知道如何验证实现吗？（测试框架、验证方式、覆盖标准）

**决策**：
- ✓ 全部打勾 → 收集完成，进入任务规划和实施
- ✗ 有未打勾 → 列出缺失信息，补充1次针对性深挖

#### 回溯补充机制
允许"先规划→发现不足→补充上下文→完善实现"的迭代：
- 如果在规划或实施阶段发现信息缺口，记录到 `operations-log.md`
- 补充1次针对性收集，更新相关 context 文件
- 避免"一步错、步步错"的僵化流程

#### 禁止事项
- ❌ 跳过步骤1（结构化快速扫描）或步骤2（识别关键疑问）
- ❌ 跳过步骤4（充分性检查），在信息不足时强行规划
- ❌ 深挖时不说明"为什么需要"和"解决什么疑问"
- ❌ 主AI自行收集代码/文档，必须委托用户使用 Codex 执行
- ❌ 上下文文件写入错误路径（必须是 `.claude/` 而非 `~/.claude/`）

## 💡 开发哲学（强制遵循）
- 必须坚持渐进式迭代，保持每次改动可编译、可验证
- 必须在实现前研读既有代码或文档，吸收现有经验
- 必须保持务实态度，优先满足真实需求而非理想化设计
- 必须选择表达清晰的实现，拒绝炫技式写法
- 必须偏向简单方案，避免过度架构或早期优化
- 必须遵循既有代码风格，包括导入顺序、命名与格式化

### 简单性定义
- 每个函数或类必须仅承担单一责任
- 禁止过早抽象；重复出现三次以上再考虑通用化
- 禁止使用"聪明"技巧，以可读性为先
- 如果需要额外解释，说明实现仍然过于复杂，应继续简化

## 🔧 项目集成规则

### 学习代码库
- 必须寻找至少 3 个相似特性或组件，理解其设计与复用方式
- 必须识别项目中通用模式与约定，并在新实现中沿用
- 必须优先使用既有库、工具或辅助函数
- 必须遵循既有测试编排，沿用断言与夹具结构

### 工具
- 必须使用项目现有构建系统，不得私自新增脚本
- 必须使用项目既定的测试框架与运行方式
- 必须使用项目的格式化/静态检查设置
- 若确有新增工具需求，必须提供充分论证并获得记录在案的批准

## ⚠️ 重要提醒

**绝对禁止：**
- 在缺乏证据的情况下做出假设，所有结论都必须援引现有代码或文档

**必须做到：**
- 在实现复杂任务前完成详尽规划并记录
- 对跨模块或超过 5 个子任务的工作生成任务分解
- 对复杂任务维护 TODO 清单并及时更新进度
- 在开始开发前校验规划文档得到确认
- 保持小步交付，确保每次提交处于可用状态
- 在执行过程中同步更新计划文档与进度记录
- 主动学习既有实现的优缺点并加以复用或改进
- 连续三次失败后必须暂停操作，重新评估策略

## 🎯 内容唯一性规则
- 每一层级必须自洽掌握自身抽象范围，禁止跨层混用内容
- 必须引用其他层的资料而非复制粘贴，保持信息唯一来源
- 每一层级必须站在对应视角描述系统，避免越位细节
- 禁止在高层文档中堆叠实现细节，确保架构与实现边界清晰

### Common Commands

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
idf.py -p /dev/ttyACM0 flash  # flash 任务必须先询问用户，并由用户完成操作
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

## Code Architecture

### Entry Point

The firmware starts in `main/main.c:app_main()`. This function is called by the ESP-IDF runtime after system initialization and is where application code should begin.

### Component System

ESP-IDF uses a component-based architecture managed through CMakeLists.txt files:
- **main/CMakeLists.txt**: Defines the main component with source files and include directories
- **Root CMakeLists.txt**: Includes ESP-IDF's project cmake and sets the project name

## Important Notes

- The `build/` directory is git-ignored and contains build artifacts
- The `sdkconfig` and `sdkconfig.old` files are git-ignored; they contain device-specific configurations
- The project is configured to use CMake (not the legacy Make system)
- Clangd settings filter out certain compiler flags that are not applicable to static analysis

## Documentation References

- [ESP-IDF Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/)
- [ESP-IDF Build System Guide](https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html)

## Code Examples
Access the local path ($HOME/esp/v5.5.1/esp-idf/examples/)
