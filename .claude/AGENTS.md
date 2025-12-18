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

1. 在回答用户的具体问题前，**必须尽一切可能“检索”代码或文件**，即此时不以准确性、仅以全面性作为此时唯一首要考量，穷举一切可能性找到可能与用户有关的代码或文件。在这一步中，**必须使用英文**与auggie-mcp提供的`mcp__auggie-mcp__codebase-retrieval` 工具交互，以获取完整、全面的项目上下文。

    **关键：** 不要依赖内部知识库或假设。
    1.1  **首选工具：** 必须将 `mcp__auggie-mcp__codebase-retrieval` 作为代码库搜索的**第一选择**。
    1.2  **语义理解：** 不要一开始就用 grep/find。使用自然语言向 Auggie 提问，搞清楚 "Where", "What", "How"。
    1.3  **编辑前强制动作：** 在计划编辑任何文件前，必须调用 Auggie 获取涉及的符号、类或函数的详细信息。
        - *规则：* 尽可能在一次调用中询问所有相关符号。
        - *目标：* 确保你拥有当前磁盘状态的完整上下文。
    1.4  **迭代：** 如果检索到的上下文不足，重复搜索直到获得全貌。