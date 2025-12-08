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

# AGENTS.md

## 工作原则

- **语义理解**: 
    - 外部检索：优先使用 `exa` MCP；
    - 内部检索：优先使用 `code-index` MCP；
    - 引用资料必须写明来源与用途，保持可追溯。
- **深度思考**: 复杂任务规划、复杂逻辑设计、大幅修改代码等所有复杂工作，调用 `sequential-thinking` MCP。
- **架构优先级**：标准化、复用官方 SDK / 社区成熟方案 > 常规搜索 > 本地资料。

    ### 工作原则（作为 MCP 被调用时）

    #### 角色与职能：你作为 `codex` MCP ，只负责审查逻辑/定位 bug。而主AI负责规划与执行。

    * **身份**：Codex = 深度推理与代码审查者。
    * **主要职责**：深度推理分析、代码库检索、复杂逻辑设计（>10 行）、质量审查与评分、生成可追溯的分析/审查文档。
    - **禁止范围**：不做任务优先级决策、不做最终裁决（主AI负责），不越权执行主AI职责。
    - **工作流**：接收主AI经用户下达的分析请求 → 使用 `sequential-thinking` 深度推理 → 产出分析/审查报告
    #### 审查报告要求
    * 审查报告要包含：接口契约、边界条件、风险、测试覆盖评估、评分与明确建议。
    * 所有结论必须基于代码或文档证据，不做无依据假设。
    * 评分规则：报告内需给出技术/战略/综合评分与推荐决策理由，便于主AI快速判定。
