# Project 4 最小 Demo：环境配置与运行指南

本文对应 `projects/project4/` 中的实验室 Go2 导航示例。
目标是从随项目提供的真实场景和导航地图重新生成动画。
所有 Python 依赖配置、运行配置、安装脚本和虚拟环境均位于本目录；不需要修改课程仓库顶层。

## 1. 获取项目与确认目录

课程仓库：[syby119/AICG-projects-answer](https://github.com/syby119/AICG-projects-answer)。

提交目标是这个仓库 **`main` 分支中原有的 `projects/project4/`**（`projects` 有复数 `s`）。
本机 `/home/qzd/Trajectory_data_generate2NavDP/course_submission/` 是该课程仓库的本地副本，
其 `origin` 指向上面的 GitHub 仓库；本机路径与仓库内路径的对应关系为：

```text
本机课程仓库根目录：.../Trajectory_data_generate2NavDP/course_submission/
仓库内项目目录：    projects/project4/
本机完整项目目录：  .../Trajectory_data_generate2NavDP/course_submission/projects/project4/
```

本示例直接补充在课程仓库已有的 `projects/project4/` 中，无需再创建一层 `project4`，
也无需把整个 `Trajectory_data_generate2NavDP` 提交到课程仓库。
`course_submission` 只是本机给仓库副本取的文件夹名，不会作为一层目录提交到 GitHub。

如果还没有接受协作邀请，请使用收到邀请的 GitHub 账号打开邀请邮件或仓库邀请页面并接受。
仓库公开可读，克隆不要求写权限；推送分支需要 Collaborator 权限和 GitHub 身份认证。

```bash
git clone https://github.com/syby119/AICG-projects-answer.git
cd AICG-projects-answer/projects/project4
```

上述命令获取 `main` 中的内容。若 demo 的提交分支已推送、但尚未合并到 `main`，应使用
`git clone -b project4/lab-go2-demo https://github.com/syby119/AICG-projects-answer.git` 获取含 demo 的分支。
最终通过 Pull Request 将提交分支合并到 `main`，文件位置仍为 `projects/project4/`。
老师原有的 `README.md`、`README-zh.md` 和 `images/` 均保留。

必要文件布局：

```text
projects/project4/
├── SETUP_GUIDE.md
├── DEMO_README.md
├── NOTICE.md
├── pyproject.toml                # Python 项目与直接依赖
├── .python-version              # Python 3.10
├── .gitignore
├── configs/                     # 全部运行路径和参数
├── scripts/
│   ├── setup_env.sh              # 仅安装到本目录 .venv
│   ├── run_demo.sh               # 统一入口
│   └── render_go2_scene_animation.py
├── src/robotnav/                 # 运行所需的最小算法子集
├── assets/
│   ├── asset_manifest.json       # 输入大小、SHA-256 和来源
│   ├── navigation/               # 地图、manifest、真实地面支持点
│   ├── scene/                    # 3 个压缩高斯分块
│   └── go2/                      # URDF、OBJ 网格和 Unitree 许可证
├── examples/                     # 对照视频、图片、验证记录
├── .venv/                       # 本地生成，不提交
├── .cache/                      # 本地依赖缓存，不提交
└── outputs/                     # 本地运行结果，不提交
```

运行不依赖提交者电脑中的其他目录、Conda 环境、训练数据包或 X-NavDP 权重。
原始约 1.1 GB 的 LAS 和约 223 MB 的完整 PLY 不属于此最小 demo 的运行输入。

## 2. 系统与必要软件

### 2.1 支持范围

- 完整动画：Linux x86_64、Python **3.10**、NVIDIA GPU 与可工作的驱动、FFmpeg。
- 已验证的 GPU 组合：RTX 4090 Laptop、PyTorch `2.4.0+cu121`、gsplat `1.5.3+pt24cu121`。
- 仅 CPU：可以重新规划及生成机器狗姿态预览；真实 3DGS 背景渲染仍需要 CUDA。
- Windows/macOS 原生 GPU 渲染不属于本示例已验证的安装范围。

首次安装建议预留至少 12 GiB 空间，用于 CUDA 版 PyTorch、依赖下载及解包。
本机安装后的 `.venv/` 约 5.7 GiB，`.cache/` 约 2.9 GiB；这些目录不提交到 GitHub。
项目输入本身约 130 MiB，运行另生成约 130 MiB 的场景缓存及 GIF/MP4。

### 2.2 Ubuntu 安装系统软件

以下命令适用于能从软件源安装 Python 3.10 的 Ubuntu（例如 Ubuntu 22.04）：

```bash
sudo apt update
sudo apt install -y git python3.10 python3.10-venv ffmpeg libgl1 libglib2.0-0
python3.10 --version
ffmpeg -version
nvidia-smi
```

系统驱动是操作系统组件，不能随 `.venv` 打包。若 `nvidia-smi` 无法列出 GPU，
先通过 Ubuntu 的驱动管理工具安装合适的 NVIDIA 驱动并重启，再继续 GPU 安装。
本项目提供匹配 PyTorch/CUDA 的 gsplat 预编译 wheel，正常安装不需要自行编译 CUDA 扩展。

软件源没有 Python 3.10 时，请先提供可用的 Python 3.10 解释器，不要直接改用 Python 3.12。
安装脚本支持 `PYTHON_BIN` 指定解释器。提交者当前电脑可以执行：

```bash
PYTHON_BIN="$HOME/miniconda3/envs/Trajectory_NavDP/bin/python" ./scripts/setup_env.sh gpu
```

这条命令仅借用现有 Python 创建本目录的 `.venv`，pip 安装目标仍是 `.venv`，不会向原
`Trajectory_NavDP` 环境安装软件。其他电脑应换成本机 Python 3.10 的位置。

## 3. 在项目目录内创建环境

以下所有命令均在 `projects/project4/` 执行。

```bash
chmod +x scripts/setup_env.sh scripts/run_demo.sh
./scripts/setup_env.sh gpu
```

脚本执行以下操作：

1. 检查 Python 必须为 3.10。
2. 在当前项目的 `.venv/` 创建隔离环境，不使用全局 pip 安装。
3. 根据 `pyproject.toml` 安装本项目和基础依赖。
4. 从 PyTorch 官方 CUDA 12.1 wheel 源、gsplat 官方 wheel 源安装已匹配的 GPU 版本。
5. 将 pip 缓存放在本目录 `.cache/pip/`。

依赖版本以 `pyproject.toml` 为准，主要组合为：

| 依赖 | 版本 | 用途 |
| --- | --- | --- |
| Python | 3.10 | 匹配预编译 GPU wheel |
| NumPy | 1.26.4 | 地图、轨迹、场景数据 |
| SciPy | 1.15.3 | 真实地面点的邻域查询 |
| PyTorch | 2.4.0+cu121 | CUDA 张量和渲染运行时 |
| gsplat | 1.5.3+pt24cu121 | 真实高斯场景 RGB/深度渲染 |
| PyBullet | 3.2.7 | Go2 模型及 CPU 外观渲染 |
| OpenCV | 4.11.0.86 | 图像合成、导航标注 |
| FFmpeg | 系统可执行程序 | MP4 编码 |

参考：[PyTorch 历史版本安装说明](https://pytorch.org/get-started/previous-versions/)、
[gsplat 官方 wheel 源](https://docs.gsplat.studio/whl/)。

检查环境：

```bash
env -u PYTHONPATH -u PYTHONHOME ./.venv/bin/python -m pip check
./scripts/run_demo.sh check
```

`check` 会校验打包文件 SHA-256、导航地图 hash、FFmpeg、NVIDIA 驱动、PyTorch CUDA 和
gsplat CUDA 扩展。GPU 检查失败时返回非零退出码，请先修复后再渲染。
无需执行 `conda activate`；启动脚本直接使用本项目 `.venv/bin/python`。
启动脚本还会隔离调用终端继承的 Python 模块路径和动态库路径，避免 ROS 或其他仿真环境影响本项目。

### 仅 CPU 的安装方式

```bash
./scripts/setup_env.sh cpu
./scripts/run_demo.sh plan
./scripts/run_demo.sh robot-preview
```

CPU 安装不包括 Torch/gsplat；完整 GPU 检查返回失败是预期情况。
之后可运行 `./scripts/setup_env.sh gpu` 在同一项目 `.venv` 中补齐 GPU 依赖。

## 4. 运行与产物

### 第一步：重新规划路线

```bash
./scripts/run_demo.sh plan
```

程序加载 `assets/navigation/scene_model.npz` 及其 manifest，校验地图完整性，
读取 `configs/trajectories_lab.toml`，执行 A*、路径简化与平滑、碰撞检查。
此命令读取的是预处理导航地图，不会重新读取原始 LAS，也不会训练模型。

主要产物：

```text
outputs/trajectories/trajectory_manifest.json
outputs/trajectories/routes/lab_000.json
outputs/trajectories/routes/lab_000_debug.png
```

```bash
xdg-open outputs/trajectories/routes/lab_000_debug.png
```

### 第二步：先渲染 5 帧预览

```bash
./scripts/run_demo.sh preview
xdg-open outputs/animation/preview_contact_sheet.png
```

程序校验输入，将 3 个无损高斯分块还原成 `outputs/cache/laboratory.ply`，
重新规划，再渲染起点、行进中和终点的 5 个指定帧。该命令验证完整渲染链路，
生成 PNG 和 `preview_report.json`，不会生成新 GIF/MP4。

### 第三步：生成完整动画

```bash
./scripts/run_demo.sh render
```

执行内容：高斯场景加载 → A* 重新规划 → 真实地面支持高度查询 →
按轨迹更新 Go2 位置、朝向和腿部关节 → 3DGS 与机器人深度合成 →
叠加规划路径、当前位置、剩余距离和到达标记 → 导出视频及报告。
运行期间每约 20 帧打印一次进度。

默认视频时长为 12 秒，离线生成耗时与设备有关，不是只能运行 12 秒。
判断完成以程序正常返回终端并打印 `Animation report:` 为准。

主要产物：

```text
outputs/animation/go2_real_scene.gif
outputs/animation/go2_real_scene.mp4
outputs/animation/animation_report.json
outputs/animation/preview_contact_sheet.png
```

```bash
xdg-open outputs/animation/go2_real_scene.mp4
xdg-open outputs/animation/go2_real_scene.gif
```

### 第四步：核对执行结果

```bash
./.venv/bin/python - <<'PY'
import json
from pathlib import Path
r = json.loads(Path('outputs/animation/animation_report.json').read_text())
for key in ['route_length_m', 'frame_count', 'final_arrived', 'final_goal_distance_m']:
    print(key, r[key])
assert r['frame_count'] == 240
assert r['final_arrived']
assert r['final_goal_distance_m'] <= 1e-6
PY
```

这验证的是预设运动学执行器走到了规划终点。它不构成真实动力学、RL 策略或实机避障成功率的验证。
无 GPU 的机器狗预览：

```bash
./scripts/run_demo.sh robot-preview
xdg-open outputs/animation/robot_only_preview.png
```

## 5. 修改起点、终点和相机

修改 `configs/trajectories_lab.toml` 中的请求，然后重新运行 `preview` / `render`：

```toml
[trajectory_batch]
count = 1
seed = 17
# 其余字段保留
requests = [
  { id = "lab_000", start_xz = [-3.9236946106, 0.4394933128], goal_xz = [-7.3636946106, 3.7194933128] },
]
```

坐标单位是米，地面为 X-Z 平面。起终点应位于地图可通行区域；规划器可能吸附到附近
自由栅格，最终实际起终点见生成的 route JSON。当前长度约束为 5–10 米，短距离任务需同时
调整 `[trajectory_sampling]` 长度范围和 `min_start_goal_distance_m`。

`configs/animation_lab.toml` 中 `[robot].trajectory_id` 必须与请求 id 一致。
`[camera]` 修改观察位置；`[video]` 修改分辨率、帧率和时长。
改帧率/时长时，`preview_indices` 必须仍在视频帧数范围内；分辨率必须容纳右下角导航图。

此包为原实验室路线裁剪了场景，地面支持点的范围为 X∈[-12,1]、Z∈[-2,6]。
改变路线或相机后，超出原视野的场景可能不完整；地面缺少真实点支持时程序明确报错。
要覆盖其他房间或走廊，应从原始数据重新提取对应场景和地面支持点，并更新资产 manifest，
不能随意编辑 hash 来掩盖数据变化。

注意：`count=3` 表示生成一个批次的 3 个起终点任务，**不表示同一起终点的 3 条候选路线**。
本最小示例默认只有一条路线。

## 6. 提交到课程仓库

本目录之外不增加依赖配置。保留老师原有说明文件和图片，在项目内新增本 demo。
不要上传 `.venv/`、`.cache/`、`outputs/`、原始大 LAS/PLY、训练数据集或训练权重。
`assets/` 是运行必需输入，必须与源码一同提交；`examples/` 是小型对照材料。

下面的命令用于本机已经准备好的课程仓库副本；不需要再次新建项目或复制文件：

```bash
cd /home/qzd/Trajectory_data_generate2NavDP/course_submission
git remote -v
git branch --show-current
git status --short
git add projects/project4
git diff --cached --stat
git diff --cached --name-only
git commit -m "feat(project4): add runnable laboratory Go2 demo"
git push -u origin project4/lab-go2-demo
```

本机已从课程仓库的 `main` 创建 `project4/lab-go2-demo` 提交分支。
其他电脑的仓库目录可能不同，应相应修改 `cd` 路径；准备新改动时可从最新 `main` 创建自己的分支。
检查暂存文件应全部以 `projects/project4/` 开头，再提交；不要提交本机绝对路径对应的额外目录层级。
HTTPS 推送需要 GitHub 登录凭据（例如凭据管理器或 Token），不能使用 GitHub 密码代替 Token；
凭据不应写进项目文件。推送后在 GitHub 页面创建目标为 `main` 的 Pull Request，供老师审阅。
提交分支的名称不会改变项目的目录位置。合并后应到仓库的 `main` 分支下检查
`projects/project4/SETUP_GUIDE.md` 和 `projects/project4/pyproject.toml` 是否存在。

## 7. 常见问题与课程范围

- `Permission denied`：运行 `chmod +x scripts/*.sh`。
- 找不到 Python 3.10：安装对应解释器，或用 `PYTHON_BIN=...` 指定后重新运行安装脚本。
- `No matching distribution found`：检查 Linux x86_64、Python 3.10 和两个官方 GPU wheel 源是否可访问；
  不要直接升级 Torch 或把 gsplat 改成源码编译来绕过版本匹配。
- `libGL.so.1` / GLib 缺失：安装前述系统库。
- `nvidia-smi` 或 CUDA 扩展失败：检查驱动、Python 是否来自 `.venv`，运行 `pip check` 后重跑 `check`。
- 内存/显存不足：关闭自己不需要的应用；可降低视频分辨率，但需同步缩小导航 inset。
  `max_cuda_memory_fraction` 只限制 Torch 分配器，不能视为整个进程的显存硬上限。
- PyBullet 提示部分辅助 link 缺少惯性参数：此处采用固定基座运动学和外观渲染；
  该提示不代表已经在进行动力学仿真。
- SHA-256 不匹配：重新获取完整 `assets/`；不要关闭完整性检查。

课程完整要求见原有 [README-zh.md](README-zh.md)。当前 demo 已包含真实重建场景加载、
实际 Go2 外观、已知障碍地图、路线规划和连续执行逻辑。
**尚未补齐**同起终点至少 3 条差异明显的候选路线与指标比较、至少 3 组测试、可配置新增障碍，
以及采集/重建过程、成员分工和 4–6 页实验报告。请在最终作业中据实补齐这些内容。
