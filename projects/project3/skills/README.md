# SCA 官方 Skill

在 project3 中运行 `uv run python demo.py install`，通过项目环境内的 `simplecadapi==2.2.3` 执行官方 `sca skill install --target opencode --skills-dir skills --force`，生成完整的 `simplecadapi/` 目录。

该目录包含官方 SKILL.md、工作流、建模规范和 API 参考，属于第三方安装产物，不提交到本仓库。SDK 自带 Skill 标注 Apache-2.0；安装时保留其许可及来源。

本项目 AGENTS.md 声明 OpenCode 原生工具与 SCA 编辑器的协作方式。建模流程由官方 Skill 提供。原始 Skill 不改写；本 demo 只运行一个 Agent，不声称提供独立视觉评审。
