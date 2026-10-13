# CPU / GPU Compute Rasterizer

从 `../rasterizer` 移植的光栅化演示，使用仓库现有 `../../base` 框架。CPU 和 GPU 共用模型变换、视锥裁剪及三角形准备，随后分别执行软件光栅化或 OpenGL compute 光栅化。GPU 依次执行分桶、可见性和 PBR 着色；两种后端的 RGBA16F 输出都通过同一全屏色调映射通道显示。

## 构建与运行

需要支持 OpenGL 4.4 的显卡驱动、CMake 3.20+ 和支持 C++20 的编译器。以下命令在仓库根目录的 PowerShell 中执行，使用 Visual Studio 2022 和仓库内的 vcpkg：

```powershell
cmake -S projects/project1 -B build/project1 -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build/project1 --config Debug --parallel
.\build\project1\Debug\Rasterizer.exe
```

Release 版本将上面的 `Debug` 替换为 `Release`。GLFW、GLAD、GLM、ImGui、stb 和 tinygltf 由项目的 vcpkg manifest 安装。

构建会把 `assets` 复制到可执行文件旁；程序从可执行文件的位置查找资源，可以从其他工作目录启动。分发时保留该 assets 目录以及 vcpkg 复制到程序旁的运行时 DLL。资源缺失或着色器编译失败会向标准错误输出具体信息并返回失败状态。

## 操作

- 默认显示 GPU compute 结果，窗口为 1280×720，开启垂直同步。
- 在窗口左上角的 `Compute Rasterizer` 面板切换 CPU / GPU compute。
- `light color`、`light intensity` 调节平行光；模型持续绕 Y 轴旋转。
- `triangles` 表示裁剪并剔除背面后的三角形数。
- `raster time` 在 CPU 模式中统计光栅化、着色及像素上传的墙钟时间，在 GPU 模式中统计三个 compute 阶段的 GPU 时间；均不包含共享场景准备和全屏显示，两个数字的统计口径不同。
- Esc 或窗口关闭按钮退出。

## 验证

在具有图形桌面及 OpenGL 4.4 驱动的环境中执行：

```powershell
ctest --test-dir build/project1 -C Debug --output-on-failure
# 或直接执行
.\build\project1\Debug\Rasterizer.exe --validate
```

验证使用相同的固定帧输入比较 CPU/GPU 输出：两个分辨率（包括非整 tile 尺寸）、两个旋转角度、部分裁剪、遮挡深度与提交顺序、空场景、零尺寸拒绝，以及实际窗口中的全屏显示、UI 和多次后端切换。检查输出有限性、alpha、覆盖与色调映射后的误差，容许浮点运算和半精度存储误差。程序输出统计并以退出码报告结果；验证结束自动退出。

本项目保持原演示的单 mesh node、单 primitive、单 PBR 材质约束；材质使用五个可选贴图槽位，软件采样与 GPU 均使用 level 0、线性过滤和 repeat 寻址。GPU 使用 16×16 tile，每个 tile 的三角形引用上限为 1024；它不是通用 glTF 查看器。

## 模型来源

`assets/models/DamagedHelmet.gltf` 来自 [Khronos glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/DamagedHelmet/glTF-Embedded)，采用内嵌 buffer 和纹理的版本。原始模型作者为 theblueturtle_，重建与 glTF 转换作者为 ctxwing。保留的 [原始许可证](assets/models/LICENSE.md) 包含 CC-BY-4.0 和 CC-BY-NC-4.0 条款；模型许可独立于仓库代码许可。

移植保留 `projects/rasterizer` 原始代码，并修正模型旋转时世界空间位置、法线和切线与投影不一致的问题。
