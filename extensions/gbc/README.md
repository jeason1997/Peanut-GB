# 可选 GBC 实验核心

移植请先读 [实现改动与移植说明](PORTING.md)：列明来源、本地精确差异、板端回调、
颜色转换、输入/RTC、内存预算与尚未完成部分。

WSL 仓库根目录构建和运行（窗口 160×144，1:1）：

```sh
python3 extensions/tools/build_peanut.py --gbc
./build-extensions/bin/peanut-sdl-gbc '女神转生白之书（CHT）.gbc'
```

固定二进制：`/mnt/e/Projects/Peanut-GB/build-extensions/bin/peanut-sdl-gbc`。
默认构建及 `build-extensions/bin/peanut-sdl` 的 DMG/屏外扩展路径不变。
GBC 前端自动按 ROM 头选择 CGB/DMG；不加载 DMG boot ROM。尚未接声音。
已接入可选扩屏；白之书没有游戏地图后端，只能使用可能过期/重复的通用 VRAM 背景。

## 银版彩色扩屏

精确日版《银》Rev1（CRC32 `0aea5383`）可复用已有地图解码，输出真实屏外地形、
当前地图精灵及相邻地图静态精灵预览，另接 CGB 彩色绘制；不是将 160×144 放大。
未修改根目录核心、原 DMG 后端或 GBC 核心。原生画面居中，窗口始终按输出像素 1:1。

WSL 仓库根目录执行：

```sh
python3 extensions/tools/build_peanut.py --gbc
# 首次准备测试存档副本；后续重用，不覆盖已玩的副本。
test -e build-extensions/silver-gbc-play.sav || cp 'Pocket Monsters Gin (Japan) (Rev 1) (SGB Enhanced) (GB Compatible).sav' build-extensions/silver-gbc-play.sav
./build-extensions/bin/peanut-sdl-gbc --viewport 320x240 'Pocket Monsters Gin (Japan) (Rev 1) (SGB Enhanced) (GB Compatible).gbc' --save build-extensions/silver-gbc-play.sav --write-save
```

`--viewport WIDTHxHEIGHT` 支持任意符合容量的整数尺寸，默认 160×144。
宽至少 160、高至少 144，单边最大 65535，最多 16777216 个像素；
SDL 纹理/主机内存不足仍会报错，不保证所有 GPU 支持所有理论尺寸。
已测 200×150、320×240、321×241、640×480、1920×1080，以及仅纵向/仅横向扩展。
原生区域位于 `((WIDTH-160)/2, (HEIGHT-144)/2)`（向下取整）。
`--terrain-only` 关闭额外精灵；`--no-background` 关闭所有屏外绘制，边缘留黑。
菜单、对话、战斗等非地图场景的银版屏外区域自动留黑，避免显示旧地图。

相邻地图切换默认开启加载追赶：保留进入切图的第一帧，再在下一次显示前运行
短暂的原游戏加载帧，直到新地图快照完整。实测往返两次的滚动间隔从5帧恢复2帧，
消除各约50ms的额外等待。每次最多额外6个模拟帧，总处理达到8ms便停止追赶。
CPU/PPU仍执行完整流程；门、传送、菜单不走此路径。RTC按显示帧对应时间计时。
`--no-transition-catchup` 可关闭以作对照；`--frames`/输入脚本按显示帧计数，额外
加载帧不递增此计数，退出时单独报告 `extra_emulated_frames`。

银版 GBC 现接入与 GB 版相同的 ROM-read NPC 激活/保留范围扩充，当前地图屏外
NPC由游戏本身运行。读取回调只覆盖精确JP Rev1的已有常量，不改磁盘ROM或核心。
未激活/邻图对象只允许静止类型（movement 1、6..9）预览；走动、转向和脚本类型
等到有真实live槽后才显示，避免猜测位置被真实位置替换。邻图不额外运行AI。
受游戏13个live槽（含玩家）和共享后端最大31格扩围限制，任意输出尺寸不代表
无限NPC模拟范围。`--no-background` 和原生160×144不启用范围扩充；
`--terrain-only`只关闭额外精灵绘制，仍保留当前地图NPC运行范围扩充。
中央原生区只补齐未送入OAM、且身体跨越画面边界的精灵部分，避免两种绘制来源
交接时缺一块；已发布的OAM姿势不重画，核心玩家/精灵像素优先保留。
跨图可见的屋顶色仍按其所属地图校正。其他原生像素保持核心输出，原 DMG 可执行文件不变。

## 1080P 性能与诊断

扩屏仍按真实输出像素绘制。32×32地图块查询按帧缓存，8个像素的图块行一次解码，
ARGB颜色查表按每条输出行检查调色板变化；不缓存跨帧NPC位置或VRAM像素。
当前桌面adapter为18008字节，比优化前增加5000字节固定缓存，输出缓冲预算不变。

`--renderer auto` 是默认值：优先可用的加速渲染，识别OpenGL的llvmpipe/softpipe
等CPU光栅器时退回SDL软件路径；`--renderer software` 或 `accelerated` 可明确选择。
启动日志会打印实际后端和可查询的GL设备。SDL标记accelerated不保证WSL实际用GPU。

原运行命令无需更改。加 `--profile` 可在退出时查看模拟与绘制、SDL呈现耗时：

```sh
./build-extensions/bin/peanut-sdl-gbc --viewport 1920x1080 'Pocket Monsters Gin (Japan) (Rev 1) (SGB Enhanced) (GB Compatible).gbc' --save build-extensions/silver-gbc-play.sav --write-save --profile
```

`run_avg_ms` 包含 `draw_avg_ms`，两者不能相加；再加 `present_avg_ms` 是每帧工作时间，
不包括限速睡眠。`profile=map` 单独统计地图场景，避免开场/菜单稀释结果。
`--hidden` 默认跳过呈现；诊断时加 `--present-hidden` 执行隐藏窗口的纹理上传和SDL呈现，
仍不等同于可见窗口/桌面合成器耗时。`--fast` 才取消59.7FPS限速。

2026-10-21同机WSL、精确银版/相同Continue输入、900显示帧（495地图帧）无插桩比较：
优化前纯模拟/绘制约14.46秒，优化后约2.79秒，最终整幅1080P截图逐字节相同。
开启实时限速和隐藏SDL呈现的1200帧测试为20.091秒、59.7FPS；地图平均模拟/绘制
3.889ms、呈现1.690ms，无超过16.743ms预算的工作帧。这里自动选择software，
因为OpenGL设备实际为llvmpipe；结果不保证其他主机/地图相同，也不外推MCU。

## 扩屏回归

真实 ROM + 存档副本回归：

```sh
python3 extensions/gbc/verify_silver_viewport.py 'Pocket Monsters Gin (Japan) (Rev 1) (SGB Enhanced) (GB Compatible).gbc' 'Pocket Monsters Gin (Japan) (Rev 1) (SGB Enhanced) (GB Compatible).sav'
```

测试只读取输入，并在 `build-extensions/gbc-silver-viewport/` 运行副本。
含 ASan/UBSan、奇数尺寸/显存银行/翻转/颜色测试，以及银版 8000 帧纯展示双实例对照
（此对照特意使用原ROM读取回调，单独隔离绘制影响）：
逐帧 CPU、WRAM、VRAM、OAM、GBC 状态与存档 RAM 相同，中央每行检查像素；
只有已识别的跨图屋顶颜色和未发布跨界精灵允许显示修正。另测昼夜、跨地图来回切换、渐暗
和精灵颜色排除，确保旧屋顶不随当前地图的调色板变化。
真实实走回归还生成 `roof-before.ppm` / `roof-after.ppm` / `roof-return.ppm`；
校验包含返回时地图编号先切换、硬件屋顶色延迟上传的过渡帧。实走脚本要求 Continue 后
group26/map3、x10/y9 的位置，其他存档明确SKIP，不计作实走通过。

精灵衔接新增 `test_sprite_seams.c`（四个边界、玩家/原生OBJ保护、已发布姿势排除）
和 `test_silver_sprite_seams.c`（吉花市实际移动路线，比较完整NPC绘制与边界合成结果）。
后者同样要求上述位置的测试存档；使用未推进RTC的确定性输入以复现同一NPC步态。
另检查真实扫描线期间的对象槽复用：精灵颜色与姿势在帧开始一起捕获，旧地图
对象按所属地图和对象编号保留颜色，防止逐行重读新槽造成半身换色或消失。
激活范围扩充后的实际往返另检查屏外live对象持续移动及进入OAM时的位置连续性；
不再将修改NPC运行范围后的游戏状态或截图宣称与原生模式一致。
当前路线通过58884个边界像素样本、3210个屏外live对象样本（14个移动样本）和23次
OAM交接，未见超过正常步幅的位置跳变；屋顶9381576个像素检查也通过，包含6163个
返回时硬件颜色滞后的样本。完整ASan/UBSan验证通过，原ROM与存档哈希未改。
截图 `320x240.ppm`、`walk.ppm`、`menu.ppm` 供人工检查。

测试源码是保留的回归保护，移植运行时无需复制：

|测试|覆盖内容|
|---|---|
|`test_core.c`|银行、DMA、调色板、双速与reset冒烟|
|`test_viewport.c`|尺寸、翻转、银行、原生区与缓冲边界|
|`test_render_cache.c`|快速图块行与逐像素参考对照、缓存碰撞/失效、逐行颜色更新|
|`test_sprite_seams.c`|四边补画、原生OBJ保护、颜色快照、预览筛选|
|`test_silver_roofs.c`、`test_silver_roof_route.c`|合成昼夜/渐暗与实际屋顶往返|
|`test_silver_viewport.c`|8000帧纯展示双实例对照|
|`test_silver_sprite_seams.c`|实际NPC范围、运动、OAM交接与颜色槽复用|
|`test_transition_catchup.c`|切图节奏、完整状态/输出对照、时间预算与菜单排除|

一次性调试探针、逐帧日志和重复试验二进制已清理。回归生成文件集中在忽略的
`build-extensions/gbc-silver-viewport/` 与 `gbc-verification/`；ROM和存档不纳入Git。

本轮实测：8000帧得到7289个有效彩色扩展帧，逐行核对1151424条原生行，
累计绘制870986个边缘OBJ像素，ASan/UBSan未报错（纯展示对照）。多尺寸、行走、
菜单另作运行检查；关闭背景扩充时仍与160×144参考截图对照。切图耗时与追赶策略的
测量方法见PORTING，主机结果不能外推MCU性能。

按键：方向键、Z=A、X=B、Enter=Start、Backspace=Select、Esc=退出。
默认**不写存档**。读取测试副本用 `--save /path/copy.sav`；明确指定
`--write-save` 才会在退出时把 cartridge RAM 写到该路径。不要指定用户原存档。
目前文件只保存 cartridge RAM，不保存 RTC 或即时状态。

隐藏验证：

```sh
python3 extensions/gbc/verify.py '女神转生白之书（CHT）.gbc'
SDL_VIDEODRIVER=dummy ./build-extensions/bin/peanut-sdl-gbc build-extensions/gbc-verification/test.gbc --hidden --frames 600
```

`verify.py` 复制 ROM 到忽略的 `build-extensions/gbc-verification`；ASan/UBSan
检查合成 MBC5/VRAM/WRAM/调色板/RGB/GDMA/KEY1/reset 测试，然后生成真实 ROM 的
room/move/menu.ppm，验证输入路径不同和存档文件往返。像素差异不是硬件准确性证明。
同时在隔离目录将原仓库 CPU 指令、指令时序、dmg-acid2 fixtures 接入可选 GBC
核心运行；三项均通过，原 DMG 核心的相同三项也通过。
输入脚本每行 `frame hex_pressed_mask`，0 起始帧，按住直到下一条记录；
A=01 B=02 Select=04 Start=08 Right=10 Left=20 Up=40 Down=80，00 为全部松开。
`--snapshot FILE.ppm` 导出原生截图；`--fast` 取消限速用于性能测量。

## 来源和本地差异

`peanut_gbc.h` 来自 https://github.com/PicoPlus-devel/pico-peanutGB
固定提交 `3b6137eac0017475b50cb259482e93096e6de11c` 的 `peanut_gb.h`。
完整 MIT 许可、Mahyar Koshkouei 与 SameBoy/Lior Halphon 归属保留在文件开头。
不使用根目录原核心作替换；也不复制 Pico 项目其他代码。

本地核心差异：两处 palette index 的 `x = ++x & mask` 改为 `x = (x+1)&mask`，
消除未定义行为；LCD timing 的条件增加花括号，行为不变。
Linux 适配宏位于 frontend.c：启用完整 CGB、Pico flash 标记为空、提供 RGB555→RGB444
转换（仅派生颜色缓存）。LCD 使用候选的 RGB555 高位 R 数据转 ARGB8888。

## 验证边界

白之书繁中 ROM SHA256 `bc7fc3e21a264fdd9af6cf82ef66c87665e5b61a5ccd92915ae6d86dab8a008a`：
12000 帧进入室内；12300 帧方向输入走到走廊；Start 输入打开角色状态界面。
以上是实际游戏推进和菜单操作，不等同于游戏内“存档→重新启动→继续”验证。
声音、完整游戏通关、HDMA/PPU/CPU 全时序一致性和游戏内继续尚未完成认证。
双速目前按候选实现调整 LCD tick，timer 使用 CPU tick；前端 RTC 用基准帧时钟。
未验证 serial fast clock/STOP 细节与 DMA 周期消耗，不宣称 cycle accurate。

`sizeof(gb_s)=50136` 字节；前端另有 92160 字节 ARGB 帧缓冲、4MiB ROM 副本和
32KiB cartridge RAM，SDL/进程资源额外计算。原 DMG context=16984 字节。
WSL CPU/RSS 测量只能说明该主机，不能推算 AC6956C8 帧率；没有修改或烧录 MCU。

2026-10-20 WSL 实测（SDL dummy 隐藏窗口、默认开场）：600 帧限速
10.046 秒，进程累计 CPU 0.129 秒（约一个逻辑核的 1.28%），峰值 RSS 12320 KiB。
12000 帧 A 输入脚本不限速约 2.1–2.35 秒，包含绘制，未包含显示或音频负担。
不能将不同时段或配置的 maxrss 值作核心内存增量。

## V3s / Linux framebuffer

独立无 SDL 前端、串口控制、构建部署命令及实机数据见 [V3S.md](V3S.md)。
默认320×240，按 fb ioctl 检查真实位域/stride/offset，运行期间临时接管 tty1。
