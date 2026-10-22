# 最小 Demo 环境与启动指南

这一版只使用 **OpenCode + SCA 官方 Skill + SCA 原生编辑器**。OpenCode 自己负责对话、提问、工具调用和继续执行；SCA 提供建模知识、执行器及模型编辑器。没有自建聊天前端、MCP 服务、会话数据库或 Docker 依赖。

## 1. 安装基础软件

需要 Python 3.12、uv，以及 Node.js（附带 npm，用于下载 OpenCode）。

- macOS：可使用 Homebrew 安装 `brew install uv node`。
- Windows：安装 uv 与 Node.js 官方发行版，重新打开 PowerShell。
- Linux：安装 uv、Node.js 和 npm；若图形依赖提示缺少 `libGL.so.1`，Debian/Ubuntu 可安装 `sudo apt install libgl1 libglu1-mesa libglib2.0-0`。

uv 可以在下面的同步步骤自动安装 Python 3.12。先确认 `uv --version`、`node --version`、`npm --version` 正常输出。

## 2. 在项目目录安装依赖

```bash
cd AICG-projects-answer/projects/project3
uv sync --python 3.12
uv run python demo.py install
```

第一次安装需下载 CAD 几何内核和可视化依赖，体积较大。`uv.lock` 固定 Python 依赖；SCA 固定为 2.2.3，OpenCode 固定为 2.0.20。

安装命令完成三件事：将 OpenCode 下载到 `.runtime/bin/`；将 SCA 自带的官方 Skill 安装到 `skills/simplecadapi/`；将 SDK 自带的 U 形支架示例复制到 `workspace/u_link/`。已有示例目录不覆盖。

所有项目配置和依赖位于 project3 内：`.venv/`、`.cache/`、`.runtime/`、`skills/`、`.env`、`workspace/`。uv 本身及系统基础软件属于系统安装；不会在课程仓库顶层新增环境配置。

## 3. 配置模型

复制 `.env.example` 为 `.env`，填写：

```dotenv
LLM_BASE_URL=https://api.deepseek.com/v1
LLM_MODEL=deepseek-v4-pro
LLM_API_KEY=你的密钥
```

可换成服务商实际支持的 OpenAI 兼容接口和模型 ID。启动脚本从 `.env` 加载密钥，仅把环境变量名写入 OpenCode 配置；不要提交 `.env`。

## 4. 启动原生模型编辑器

在第一个终端、project3 目录运行：

```bash
uv run python demo.py editor
```

打开 <http://127.0.0.1:8765>，从文件列表打开 `u_link/u_link.py` 或其它带 SCA 元数据的 notebook。界面、参数、选面、代码单元和执行日志均由 SCA 提供。

## 5. 启动 Agent

在第二个终端、同一项目目录运行：

```bash
uv run python demo.py agent
```

这是 **OpenCode 原生终端界面**，不是旧版网页。启动器使用独立的原生服务和项目内 XDG 目录，避免混用其它项目的配置。依照 OpenCode 提示处理工具权限；这里没有增加自定义审批逻辑。

可以先输入：

> 加载 simplecadapi Skill，查看编辑器中打开的 U 形支架及其参数，解释它的建模步骤。先不要修改。

然后输入：

> 把这个支架的一个可调尺寸改大 5 mm，其余保持不变。先读取当前参数，用编辑器协作方式修改，运行并检查结果。

也可以从零描述一个简单零件：

> 制作一块 80×50×5 mm 的矩形板，中央一个直径 8 mm 的通孔，单位 mm。保存参数化 notebook 并导出 STEP。

Agent 直接使用原生文件和命令行工具：读取 Skill → 必要时提问 → 编写/修改 notebook → 执行并读取反馈 → 修复 → 导出。不是预设模板或单次模型调用。编辑器打开时，Skill 指导 Agent 使用 `sca snapshot`、`sca cell` 协作，避免直接覆盖编辑器持有的文件。

模型 notebook、导出文件和记录都在 `workspace/`。这是本地原生执行，文件操作遵循 OpenCode 自身的权限提示；不再提供旧版 Docker 隔离与独立验收服务。Agent 应使用 SCA 检查实际结果，不声称执行了未做的验证。

## 6. 结束与常见问题

- 编辑器在所属终端用 Ctrl+C 停止；Agent 的中断、退出和历史会话使用 OpenCode 自带操作。
- `8765` 被占用：停止已有编辑器再启动。
- 未找到 `sca` 或依赖：确认在 project3 目录执行 `uv sync`，并用 `uv run python demo.py ...`。
- 未找到 Skill/OpenCode：重新执行 `uv run python demo.py install`。
- 模型认证失败：核对 `.env` 的地址、模型与密钥，然后退出并重新启动 Agent。
- 编辑器空白：从文件列表选择实际 notebook；不要把旧版网页的端口 `8503` 当作原生编辑器。

## 7. 旧版与其它 harness

原课程设计文档保留。旧版自建网页、工具服务、测试和模型记录已在本机备份到 `.runtime/legacy-v1-20261022/`，不会作为新版启动依赖，也不提交到仓库。

这里用 OpenCode 仅作演示。官方 Skill 支持其它 harness 安装目标；可用 `uv run sca skill targets` 查看。换用 Claude Code、Codex 等时，让它读取同一套 Skill 并操作相同的 notebook/编辑器即可，模型认证和工具权限按对应 harness 配置。本 demo 只验证 OpenCode 这一条接入。
