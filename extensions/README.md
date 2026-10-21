# 屏外画面扩展实验（GX）

新增独立、可选的实验性 GBC 原生前端：`python3 extensions/tools/build_peanut.py --gbc`，
固定输出 `build-extensions/bin/peanut-sdl-gbc`。来源、操作、验证与未完成项目见
[GBC 说明](gbc/README.md)。默认 DMG/屏外构建保持原路径。
GBC 版已支持 `--viewport 320x240` 等任意合法尺寸，精确日版银Rev1支持彩色地形及
屏外精灵及当前地图NPC运行范围扩充；移动NPC使用真实live状态，尚未激活的移动
NPC不再作位置猜测预览。使用方法与验证见上述 GBC 说明。

这是可移植的 **presentation extension**，不是把 GB/FC 的硬件分辨率改大。
目标是保留原始 CPU/PPU/中断时序，在前端合成额外画面；特定游戏可选运行时 ROM 读取补丁。

## 直接编译扩展版（WSL/Linux）

在仓库根目录执行：

```sh
python3 extensions/tools/build_peanut.py
# 或 make -C extensions
./build-extensions/bin/peanut-sdl --viewport 200x150 "ROM文件" "存档副本"
```

依赖：Python3、Git、make、C编译器、SDL2开发包。脚本使用固定上游基线的临时源码，复制本模块并应用集成Patch；不修改主目录源码。
**二进制固定在 `build-extensions/bin/peanut-sdl`**，临时源码在构建完成后清理。窗口启动1:1。
旧命令 `./examples/sdl2/peanut-sdl --viewport ...` 指向恢复后的原版，原版没有这些扩展参数；请使用上面的新路径。
需要旧回归可执行文件：`python3 extensions/tools/build_peanut.py --with-tests`，输出 `build-extensions/bin/test_silver_jp`。
SDL退出可能写存档，请传入副本；不提供存档参数时会使用ROM同名存档。

## 工作区布局：上游保持原样

主目录的 `peanut_gb.h`、`examples/`、`test/`、README 已恢复上游基线。
扩展源码、文档、独立测试、可选接入补丁全部位于 `extensions/`。
本目录里的测试用于验证扩展；原 `test/` 不再添加或修改任何测试。
`patches/0002-peanut-integration.patch` 和 `0003-regression-tests.patch` 是封装的可选改动，当前未应用。
不要因为主目录恢复，就把扩展能力误认为丢失；请在临时检出应用补丁运行实验。

## 目前真正支持什么

|层级|已实现|边界|
|---|---|---|
|通用画面合成|独立 C99 单头文件，任意原始/输出尺寸、逐行输出、调用者提供缓冲|输出宽高必须不小于原始；不负责发现游戏世界|
|通用 GB 背景|DMG VRAM tilemap 扩展，不运行额外 PPU 扫描线|VRAM 屏外内容可能过期；不保证真实周边地图，不支持 CGB bank/属性|
|游戏适配|日版《宝可梦 银》Rev 1，完整地形、当前地图额外 NPC、边界填充、相邻 NPC 预览|仅精确指纹；不是《金》《红》的适配|
|模拟器接入|Peanut-GB 原生行回调零核心修改示例；现有 SDL 接入补丁|其他模拟器须实现状态视图和前端回调|
|FC/NES|合成层以 256×240 为原始尺寸的单元测试|尚无 NES PPU/mapper 后端或 NES 游戏适配；不能宣称已支持 FC 游戏|
|嵌入式|资源容量可编译时缩减，功能可运行时关闭|AC6956C8 尚未接入/编译/烧录；没有板上性能承诺|

## 文件入口

- `include/gx_viewport.h`：与模拟器、主机和游戏无关的画面合成。
- `include/gx_gb_view.h`：借用 GB 内存和寄存器的只读状态视图。
- `include/gx_gb_background.h`：通用 DMG 背景后端。
- `games/pokemon_silver_jp.h`：《银》适配唯一实现。
- `ports/peanut-gb/gx_peanut_view.h`：Peanut-GB → 中立状态视图转换。
- `../examples/common/pokemon_silver_jp.h`：应用0002补丁后才创建的薄兼容入口。
- `tests/test_portable_silver.c`：只接原始 LCD 回调的完整可运行示例，无需核心 viewport API。
- [完整移植与实验文档](PORTING.md)。
- [接手清单](HANDOFF.md)。
- [补丁说明](patches/README.md)。

## 在 WSL 运行验证

在仓库根目录：

```sh
sh extensions/tests/run.sh
sh extensions/tests/run.sh '/path/to/Silver.gb' '/path/to/read-only-input.sav'
# 以下 SDL 命令须在应用可选补丁的临时检出中运行：
make -C examples/sdl2 OS=Linux -j2
```

前两个测试不写 ROM/存档，不创建窗口。SDL 游戏会保存进度，调试时务必指定存档副本。
SDL 默认窗口 1:1；`--viewport 200x150` 是逻辑像素，不是缩放倍数。

## 功能与预算

`pgb_silver_jp_configure(state, feature_mask, object_limit)` 在帧之间调用，返回 1 表示成功。
拒绝未知位、非法依赖或非法上限，失败不修改配置。默认全部功能保持旧实验行为。

|位/值|标志|控制|
|---|---|---|
|0 / 1|`PGB_SILVER_TERRAIN`|游戏地图重建|
|1 / 2|`PGB_SILVER_SPRITES`|额外精灵绘制/对象准备|
|2 / 4|`PGB_SILVER_NPC_LIFETIME`|扩大游戏当前地图 NPC 生命周期；包括防 OAM 坐标回绕补丁|
|3 / 8|`PGB_SILVER_FACING_MEMORY`|运行时读 ROM 恢复离屏 NPC 朝向|
|4 / 16|`PGB_SILVER_CONNECTIONS`|相邻地图接续信息|
|5 / 32|`PGB_SILVER_STABLE_BORDER`|连通区域空间归属与稳定水/树填充|
|6 / 64|`PGB_SILVER_NEIGHBOR_NPCS`|相邻地图 NPC 静态预览|

依赖：精灵、朝向记忆或地图接续需要 TERRAIN；稳定边界和相邻 NPC 需要 CONNECTIONS；相邻 NPC 还需要 SPRITES。
NPC_LIFETIME、FACING_MEMORY 可分别关闭；关闭精灵绘制并不自动关闭 NPC AI 扩围，省 CPU 时应明确关闭生命周期位。
没有启用生命周期扩围时，原屏外对象可能只是未激活对象的静态绘制，不能承诺屏外 AI。
`sprites_enabled` 是保留的旧 F9 总开关，只控制额外精灵展示；配置位仍为最终约束。保留朝向记忆时仍须观察当前对象，即使精灵绘制关闭也保留这部分工作；极低配应同时关闭朝向记忆。
关闭 TERRAIN 时游戏后端返回“未绘制”，调用方决定留白或使用通用 VRAM 后端；SDL 当前留白。

SDL 新选项（十进制或 `0x` 十六进制）：

```sh
./examples/sdl2/peanut-sdl --viewport 200x150 --extension-features 127 --extension-objects 48 ROM SAVE_COPY
# 仅当前地图地形：1；地形+相邻接续+稳定填充：49
./examples/sdl2/peanut-sdl --viewport 200x150 --extension-features 49 ROM SAVE_COPY
# 展示功能全关，仍保留配置的留白画布
./examples/sdl2/peanut-sdl --viewport 200x150 --extension-features 0 ROM SAVE_COPY
```

`--terrain-only`/F9 是展示开关，不等同于将 features 设置为 1。
F8 切换原始/扩展尺寸；原始尺寸时 ROM 扩围补丁停用。

编译容量（在任何适配头之前定义，并对同一构建各 TU 使用一致设置）：

```c
#define PGB_SILVER_MAX_OBJECTS 48 /* >=28，包含玩家/当前地图候选与邻图预览 */
#define PGB_SILVER_MAX_MAPS 32    /* 1..64，连通地图图谱容量 */
#define PGB_SILVER_BLOCK_CACHE 0 /* 0 或 16；0 节省缓存内存 */
#define PGB_SILVER_DEFAULT_FEATURES PGB_SILVER_TERRAIN
```

`config/terrain_small.h` 提供仅地形、28对象/1地图/无块缓存的保守起点；在所有适配头之前 include。它不是板上性能认证。

运行时 `object_limit` 必须在 28..MAX_OBJECTS；只减少工作/预览数量，不释放编译时数组。
容量满时保留当前地图对象，后续邻图预览按遍历顺序截断；`objects_dropped` 记录本次准备被省略的候选数。
`atlas_truncated` 表示图谱触及容量；缩小 MAX_MAPS 可能破坏远处填充跨地图稳定性，不能视为无损优化。
默认相邻分量测试需要 27 张 Johto 地图；已知其他分量最大 35，64 为原实验余量。

## 分发为 Patch

```sh
python3 extensions/tools/export_patches.py --output /tmp/gx-package
```

生成独立模块、Peanut-GB 接入、回归测试三个补丁和 SHA256 清单。
补丁基线锁定仓库提交 `d0bcca771c83a2638c93a9ae61f3d51f226dc905`，模块可提交到该基线的后续分支；导出时仍验证主目录核心未应用实验补丁。
导出工具直接读取归档接入/回归补丁，不依赖主目录先被修改。
工具不会打包 ROM、存档、二进制、SDK 或 MCU 项目，也不会写原 ROM。
其他模拟器优先复制 `extensions/` 并接回调；无需应用 Peanut 专用补丁。
