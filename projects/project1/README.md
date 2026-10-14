# CPU / GPU Rasterizer（OpenGL 4.1）

使用仓库 `../../base` 框架的自行光栅化演示。CPU 在 C++ 中变换顶点，GPU 使用顶点 shader 和 Transform Feedback；两个后端共用 CPU 视锥裁剪及三角形准备，随后分别执行软件光栅化或片元着色器光栅化。项目请求 OpenGL 4.1 Core，所有着色器使用 GLSL 410；GPU 后端不依赖 compute shader、SSBO、image load/store 或 OpenGL 4.2+ 扩展。

## 渲染管线与职责

`RasterScene` 保存 glTF 场景的根节点、子节点、mesh primitive 与材质。遍历节点时按 `parent * local` 累积变换，生成包含原始 Mesh、材质和节点世界变换的 `DrawItem` 列表；多个节点引用同一 mesh 时共用几何数据。应用层一次调用 `render(draws, frame)` 提交整组绘制项。两个后端都在整组图元上统一处理深度，按图元所属材质着色。

两个后端在自己的 `render()` 内执行顶点阶段：CPU 调用 C++ 辅助函数，GPU 把原始顶点及矩阵交给 `transform.vert`。顶点位置、法线、世界坐标和 UV 以相同的 `TransformedVertex` 布局输出，随后调用共享的 CPU 裁剪和三角形准备函数。每个绘制项的输入顶点只变换一次，再根据索引组装图元；`PreparedTriangle` 是各后端内部的三角形中间数据，不作为渲染器提交接口的输入。CPU 后端在软件中插值和着色，GPU 后端使用下面的 tile 列表及片元着色器。应用层只负责窗口、交互、相机与光照参数，以及 HDR 结果的色调映射和显示。

## GPU 后端的数据流

原始顶点写入 VBO，顶点 shader 按 glTF 节点变换、实例变换、相机和投影处理每个顶点，并使用逆转置矩阵变换法线。该通道通过 RHI 绘制 Points，开启 `GL_RASTERIZER_DISCARD`，由 Transform Feedback 在硬件裁剪之前捕获四个 vec4 输出。结果读回 CPU，用于齐次裁剪、图元组装、透视除法、视口变换、背面剔除和 TBN 构建。读回会同步等待 GPU；这一版是混合管线，CPU 不再为 GPU 后端重复计算顶点变换。

CPU 将整组绘制项的三角形按 16×16 tile 分桶，生成 `(offset, count)` 和共享的变长索引数组，保持提交顺序。GPU 可见性片元着色器逐像素遍历所属 tile 的列表，自行计算覆盖、重心坐标和深度，写入 R32UI 三角形 ID。PBR 片元着色器按材质对应的三角形范围逐次绘制，只为范围内胜出的图元写入 RGBA16F。两种后端通过同一全屏色调映射通道显示。

所有全屏通道使用空 VAO 和 `gl_VertexID` 生成的单个全屏三角形，无需 VBO 或顶点属性。硬件只为这些全屏绘制产生片元；模型三角形的覆盖与深度仍由项目代码计算。数据放入 RGBA32F / RG32UI / R32UI 二维纹理，用 `texelFetch` 读取，避免依赖较大的 Buffer Texture 容量。每个 tile 不再有 1024 个三角形的上限，也不会因固定列表满了而丢弃三角形。总数据量仍受内存、32 位索引和 `GL_MAX_TEXTURE_SIZE` 限制；超过二维纹理容量会明确报错。

## Windows 构建与运行

需要 OpenGL 4.1 显卡驱动、CMake 3.20+ 和支持 C++20 的编译器。以下命令在仓库根目录的 PowerShell 中执行，使用 Visual Studio 2022 和仓库内的 vcpkg：

```powershell
cmake -S projects/project1 -B build/project1 -G "Visual Studio 17 2022" -A x64 "-DCMAKE_TOOLCHAIN_FILE=$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build/project1 --config Debug --parallel
.\build\project1\Debug\Rasterizer.exe
```

Release 版本将上面的 `Debug` 替换为 `Release`。GLFW、GLAD、GLM、ImGui、stb 和 tinygltf 由项目的 vcpkg manifest 安装，GLAD 的核心 API 配置为 4.1。保留扩展声明供共享 base 编译使用；project1 的运行路径只使用 4.1 核心功能。

## macOS 构建与运行

使用支持 C++20（包括 `std::format`）的 Apple Clang/libc++，建议 Xcode 16 或更新版本，并准备 CMake 与 vcpkg。以下命令在仓库根目录执行，假定 vcpkg 位于仓库内：

```bash
./vcpkg/bootstrap-vcpkg.sh
cmake -S projects/project1 -B build/project1-macos \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/vcpkg/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx
cmake --build build/project1-macos --parallel
./build/project1-macos/Rasterizer
ctest --test-dir build/project1-macos --output-on-failure
```

Intel Mac 将 triplet 改为 `x64-osx`。共享窗口框架已在 macOS 请求 forward-compatible Core Profile。渲染目标按实际 framebuffer 像素尺寸分配，兼容 Retina 缩放。macOS 原生构建和运行需要在 Mac 上验证。

构建会把 `assets` 复制到可执行文件旁；程序从可执行文件的位置查找资源，可以从其他工作目录启动。分发时保留 assets 和程序依赖的动态库。资源缺失或着色器编译失败会向标准错误输出具体信息并返回失败状态。

## 操作与性能统计

- 默认显示 GPU fragment 结果，窗口为 1280×720，开启垂直同步。
- 在 `Rasterizer` 面板切换 CPU / GPU fragment。
- `light color`、`light intensity` 调节平行光；模型持续绕 Y 轴旋转。
- `triangles` 表示裁剪并剔除背面后的三角形数。
- `geometry time` 统计几何准备的墙钟时间：CPU 模式包含顶点变换和裁剪；GPU 模式包含顶点上传、GPU 顶点通道、结果读回等待和 CPU 裁剪。
- GPU 模式的 `GPU vertices` 使用 GPU 时间查询，单独统计顶点 shader 与 Transform Feedback 捕获耗时。
- CPU 模式的 `raster time` 包含软件光栅化、着色及像素上传的墙钟时间；GPU 模式统计可见性和每个材质范围的 PBR 绘制的 GPU 时间。
- GPU 模式的 `CPU binning` 单独显示 CPU 分桶耗时。`raster time` 不包含几何准备、CPU 分桶和输入纹理上传，两种后端的统计口径不同；均不包含最终显示。
- Esc 或窗口关闭按钮退出。

## 验证

在具有图形桌面及 OpenGL 4.1 驱动的环境中执行：

```powershell
ctest --test-dir build/project1 -C Debug --output-on-failure
# 或直接执行
.\build\project1\Debug\Rasterizer.exe --validate
```

验证从原始 Mesh 输入开始比较完整 CPU/GPU 管线输出：两个分辨率（包括非整 tile 尺寸）、两个旋转角度、层级 glTF 场景的多个节点、primitive、材质和重复 mesh 引用、侧面与近平面裁剪、全部裁掉的 Mesh、遮挡深度与提交顺序、等深度的首次提交规则、每 tile 1301 个三角形且最近三角形最后提交、节点变换与等价实例变换、空场景、零尺寸和无效索引拒绝，以及实际窗口中的显示、UI 和多次后端切换。检查有限性、alpha、覆盖与色调映射后的误差，容许浮点运算、纹理采样和半精度存储误差。程序输出统计并以退出码报告结果；验证结束自动退出。

当前支持 glTF 场景的多个根节点与子节点、重复 mesh 引用、一个 mesh 的多个三角形 primitive，以及每个 primitive 对应的 PBR 材质。primitive 需要 POSITION、NORMAL 和 TEXCOORD_0；纹理仍限于内嵌资源。材质使用五个可选贴图槽位，CPU 与 GPU 均使用 level 0、线性过滤和 repeat 寻址。

## 模型来源

`assets/models/DamagedHelmet.gltf` 来自 [Khronos glTF Sample Assets](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/main/Models/DamagedHelmet/glTF-Embedded)，采用内嵌 buffer 和纹理的版本。原始模型作者为 theblueturtle_，重建与 glTF 转换作者为 ctxwing。保留的 [原始许可证](assets/models/LICENSE.md) 包含 CC-BY-4.0 和 CC-BY-NC-4.0 条款；模型许可独立于仓库代码许可。
