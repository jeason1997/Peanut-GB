# 给下一位 AI 的接手入口

## 可选 GBC 彩色扩屏（2026-10-21 整理）

后续1080P性能优化：`viewport.h` 加128条32×32地图块缓存（每帧清空）和逐行检查的
ARGB查表；`background.h` 一次解码8像素bitplane行，实时读VRAM，不能把图像跨帧
缓存。adapter增加5000字节至18008。SDL默认auto，识别llvmpipe等CPU GL光栅器
则回退software；新增 `--renderer`、`--profile`、`--present-hidden` 诊断选项。
同一900帧1080P纯绘制从14.46秒降至2.79秒，最终PPM逐字节一致；1200帧含隐藏SDL
呈现、正常限速为20.091秒/59.7FPS，地图工作均值约5.58ms，无超预算帧。隐藏呈现
不含桌面合成器最终延迟。新增 `test_render_cache.c` 的标量对照，完整回归已含1080P。
本轮性能优化随本次提交交付；下列运行策略与功能边界继续有效。

运行入口见 [GBC README](gbc/README.md)，实现来源、地址证据与移植接线见
[PORTING](gbc/PORTING.md)。分支 `codex/portable-viewport-extension`，推送远端 `fork`。
用户已授权本轮清理、更新文档、提交及推送。

- `peanut_gbc.h` 使用已固定来源的单头候选核心；本轮未改核心或旧DMG模块/补丁。
- `--viewport WIDTHxHEIGHT` 输出彩色扩屏；精确银JP Rev1有地形/对象后端，未知ROM
  仅通用VRAM背景。屏幕居中、1:1输出；默认160×144。没有声音和RTC文件持久化。
- `viewport.h` 接共享合成器和地图解码；`background.h` 处理CGB银行/属性/翻转。
- `silver_sprites.h` 按帧捕获颜色与姿势、处理CGB优先级，保护原生OBJ；只给身体
  跨界且未发布OAM的对象补中央缺口。旧地图颜色按地图及对象编号保留。
- ROM读取接共享NPC_LIFETIME/FACING_MEMORY及OAM回绕保护。当前地图NPC真实运行；
  未激活/邻图仅预览movement1、6..9静止对象，移动NPC不再猜位置。邻图无独立AI；
  游戏13个live槽（含玩家）和每轴31格pad有限，任意输出尺寸不代表无限模拟。
- `silver_roofs.h` 正常BGP=$E4阶段按空间所属地图和帧昼夜选择屋顶色，包括返回时
  地图编号已更新、硬件颜色尚未上传的帧。中央屋顶色与跨界精灵是两类显示修正。
- `frame_runner.h` 只处理已知连接图，且JP Rev1 hMapEntryMethod=$FFA1为$F7：
  保留进入首帧，下一显示前追赶原加载过程，最多额外6帧/8ms追加预算（单帧不可抢占）。
  RTC按显示时间推进；门/warp/菜单不触发，`--no-transition-catchup` 可作对照。

验证入口：

```sh
python3 extensions/gbc/verify_silver_viewport.py ROM SAVE
python3 extensions/gbc/verify.py
sh extensions/tests/run.sh
```

真实路线要求Continue后在吉花市group26/map3、x10/y9；其他存档明确SKIP。
8000帧原版对照专门隔离纯展示，使用原ROM读取；生产回调另测NPC移动/交接。
本轮实走58884个边界像素、3210个屏外live样本、14个屏外移动样本、23次OAM交接；
屋顶9381576个像素（含6163个返回滞后样本）通过。切图双实例比较完整执行后的CPU、
内存、OAM、调色板、存档和输出一致；往返共追赶6帧，滚动间隔5→2显示帧。
不得把扩围后的游戏状态宣称与原版一致，也不得把主机耗时外推到MCU。

临时探针/日志/重复试验二进制不纳入源码；正式test_*.c保留为回归保护。
生成文件统一位于忽略的build-extensions/；ROM、存档和用户游玩副本不得提交或清理。
白之书已验证室内/走廊/状态菜单；声音、完整通关、游戏内存档后重启继续、完整
HDMA/PPU/CPU时序尚未认证。MCU项目本轮未修改、未烧录。

先读 README.md 与 PORTING.md，再改代码。

**实验代码集中在 extensions/。不要直接修改根目录核心、examples/、test/ 来接实验。**
零核心改动示例直接用原始LCD回调；需要旧SDL实验时，在临时工作目录应用可选补丁。
接入/回归的可选差异唯一来源为 patches/0002 与 0003；修改它们须在临时基线应用、修改、重新生成并验证，不能要求主工作区长期保留已应用状态。
导出工具会将独立模块与归档补丁一起打包。当前实现不是任意游戏/任意模拟器/FC的现成通用补丁。

## 当前事实

- 独立通用合成器 + DMG VRAM fallback + 精确日版银Rev1后端。
- 默认功能保留，旧 examples/common 仅兼容转接；游戏实现唯一来源是 games/pokemon_silver_jp.h。
- 零核心改动的真实ROM接入示例在 tests/test_portable_silver.c；已有SDL使用可选核心viewport端口。
- 所有新增构建/验证用WSL；窗口只能隐藏或1:1；用户工作时不弹大窗。
- 不改用户ROM/存档；实际SDL用 /tmp 存档副本。
- MCU工程 E:/Projects/MCU_Project/AC6956C8 只做过只读调研，本次未移植、未烧录。
- 金/红与NES后端未实现；未知游戏只能通用VRAM扩展或留白，不能承诺正确地图。

## 后续优先级

1. MCU需要受控ROM读取/span缓存，当前游戏后端仍要求连续ROM地址。
2. 在目标板按分项时间/缺页/栈高水位定预算，不用WSL百分比外推。
3. 针对200×150收紧对象/地图容量；先地形，再按实际需求开NPC与邻图。
4. 无12colour标签时OAM遮挡回读较贵；研究复用宿主原始OBJ覆盖掩码，不要误覆盖玩家。
5. 新游戏先指纹与地图证据，再新后端；无证据不猜常量。
6. 带冲突连接的地图图谱还需专门处理，不宣称所有分量无缝。

## 日常构建入口

`python3 extensions/tools/build_peanut.py --with-tests` 在WSL构建，固定输出 build-extensions/bin/peanut-sdl 与 test_silver_jp。
不要把原目录 examples/sdl2/peanut-sdl 误当扩展版；不需要为了构建而改回主目录源码。

## 必须复现的验证

```sh
sh extensions/tests/run.sh
sh extensions/tests/run.sh ROM SAVE
# 下列命令在应用可选补丁的临时检出执行：
make -C examples/sdl2 OS=Linux -j2
make -C test test_silver_jp
./test/test_silver_jp ROM SAVE --scene-check
```

完整8000帧测试需要原始NewBark测试存档；当前用户存档是Cherrygrove，适合 --scene-check 与连接地图实走回归。fixture如已不存在应明确说明，不能伪造PASS。
测试CPU/PPU不变时，基线应使用相同运行时NPC策略；另测所有功能关闭时与原版一致，避免把游戏逻辑改变误说成纯展示。

## 配置注意

features是工作开关，MAX_*是保留内存；object_limit不是活动AI槽位数量。
对象不足会截断邻图预览；地图不足可能影响填充稳定；查看 objects_dropped/atlas_truncated。
更改配置在帧之间调用configure并失效快照。F9/terrain-only只关展示，不关NPC逻辑扩围。
当前邻图NPC不运行AI，跨地图卸载后不永久保存其位置。
