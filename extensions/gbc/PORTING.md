# GBC 实现改动与移植说明

本文对应 `extensions/gbc/peanut_gbc.h`，供从原 Peanut-GB 移植到其他宿主/MCU 时使用。

## 1. 实现关系：哪些代码是谁增加的

GBC 核心仍是一个单头文件：在 Peanut-GB 架构上增加 CGB 功能。
本项目**采用已有 GBC 分支**，不是我们从当前根目录 `peanut_gb.h` 开始逐项实现 GBC。
请区分分支已有的硬件实现与我们本次的集成修改，不能把两者混为一个小补丁。

|来源|版本|用途|
|---|---|---|
|本仓库原 DMG 核心|`d0bcca771c83a2638c93a9ae61f3d51f226dc905`|根目录 `peanut_gb.h` 保持原样|
|GBC 核心来源|`PicoPlus-devel/pico-peanutGB`，提交 `3b6137eac0017475b50cb259482e93096e6de11c`|取其 `peanut_gb.h`，本地命名 `peanut_gbc.h`|
|本项目本地修正|见下节及 `local-core.patch`|修正两处未定义行为、整理一处条件语句|

源仓库：https://github.com/PicoPlus-devel/pico-peanutGB

GBC 分支已经包含：CGB 模式检测、WRAM/VRAM 银行、彩色 BG/OBJ 调色板、
图块属性和绘制路径、KEY1 双速切换，以及 GDMA/HDMA 等代码。
这些功能存在于源码不代表所有硬件行为都已验证准确。

头文件开头的 MIT 许可、Mahyar Koshkouei 与 SameBoy/Lior Halphon 归属必须保留。
不需要复制整个 Pico 项目或引入 Pico SDK。

## 2. 本次实际新增/修改文件

以下路径均相对于仓库根目录。

|文件|本次作用|移植时是否需要|
|---|---|---|
|`extensions/gbc/peanut_gbc.h`|独立 GBC 模拟核心，源分支加本地修正|需要|
|`extensions/gbc/local-core.patch`|相对上述固定来源头文件的精确本地差异|审阅/重建来源时使用|
|`extensions/gbc/frontend.c`|WSL/SDL 的 ROM、RAM、输入、彩色行输出、限速、截图、存档接线|参考接线，板端重写|
|`extensions/gbc/viewport.h`|GBC/DMG → 通用合成器接线、银版 CGB 地形颜色和场景校验|需要扩屏时使用|
|`extensions/gbc/background.h`|通用 CGB 双银行/属性/翻转的 BG 像素读取|需要通用 CGB VRAM 背景时使用|
|`extensions/gbc/silver_sprites.h`|精确银版的彩色屏外精灵、OBJ 调色板与 CGB 优先级|需要银版彩色精灵时使用|
|`extensions/gbc/silver_roofs.h`|屋顶按空间所属地图选择配色，防止跨图改色|银版彩色扩屏需要|
|`extensions/gbc/frame_runner.h`|相邻地图加载帧追赶，缩短滚动停顿|需缩短切图停顿时接入|
|`extensions/tools/build_peanut.py`|新增 `--gbc` 分支，单独编译前端|板端可不用|
|`extensions/gbc/test_core.c`|银行、颜色、GDMA、KEY1、reset 冒烟测试|验证时参考|
|`extensions/gbc/test_*.c`（其余）|尺寸、屋顶、NPC/颜色交接、完整状态及切图节奏回归|仅测试，不放入板端运行时|
|`extensions/gbc/verify.py`|隔离运行 sanitizers、原 DMG fixtures、真实 ROM 输入及文件往返|桌面验证用|
|`extensions/gbc/verify_silver_viewport.py`|统一运行银版彩色扩屏回归，保护输入ROM/存档|桌面验证用|
|`extensions/gbc/README.md`、`PORTING.md`|运行入口、证据、移植说明|保留参考|
|`extensions/README.md`、`extensions/HANDOFF.md`|增加 GBC 入口和当前边界|索引|

根目录 `peanut_gb.h`、`examples/`、`test/` 没有为此修改。
已有 DMG 屏外集成补丁也未修改。默认构建不切换到新核心。

### 核心只有三处本地代码差异

1. `__gb_write` 的 BG 调色板自增：
   `BGPaletteID = (++BGPaletteID) & 0x3F` 改为
   `BGPaletteID = (BGPaletteID + 1) & 0x3F`，避免同一表达式重复修改对象的未定义行为。
2. OBJ 的 `OAMPaletteID` 同样修正。
3. `__gb_step_cpu` 的 LCD tick 条件分支改用显式花括号/`#else`，保持原行为，消除误导缩进警告。

精确差异见同目录 `local-core.patch`，可对固定来源 `peanut_gb.h` 应用；
该补丁不是把当前原版 DMG 核心升级为 GBC 的完整补丁。

## 3. 板端最小接入

在一个 C 编译单元中，先定义配置宏，再包含核心：

```c
#define PEANUT_FULL_GBC_SUPPORT 1
#define ENABLE_SOUND 0
/* 非 Pico 平台：去掉放置到 RAM 的标记。 */
#define __not_in_flash_func(name) name
/* 此分支保留一个 RGB444 派生缓存，即使面板不用它也需定义。 */
#define RGB555_TO_RGB444(c) \
    ((((c) >> 3) & 0xF00) | (((c) >> 2) & 0xF0) | (((c) >> 1) & 0x00F))
#include "peanut_gbc.h"

static struct gb_s gb; /* 避免把约 50KB context 放在任务栈上。 */
```

宏可以按目标平台调整。不要同时包含原 DMG 与 GBC 头文件：二者都有
`PEANUT_GB_H` include guard、`struct gb_s` 和同名函数。默认会生成函数定义，
多编译单元组织须按该头文件的 `PEANUT_GB_HEADER_ONLY` 声明模式处理。

核心只需标准 C 环境；SDL2、Python、Linux `getrusage`、动态 ROM 缓冲和
ARGB 帧缓冲均属于桌面前端，不是核心依赖。

### 必须提供的回调与初始化顺序

按 `frontend.c` 的签名实现：

|回调/步骤|约定|
|---|---|
|ROM read：`uint8_t (struct gb_s *, uint_fast32_t addr)`|`addr` 是核心完成银行映射后的 ROM 字节地址，宿主不要再套一次当前 ROM bank|
|RAM read：`uint8_t (struct gb_s *, uint_fast32_t addr)`|读取 cartridge RAM，不是内部 WRAM|
|RAM write：`void (struct gb_s *, uint_fast32_t addr, uint8_t value)`|更新 cartridge RAM；可标脏后另择时机落盘|
|error：`void (struct gb_s *, enum gb_error_e, uint16_t addr)`|记录并停止/恢复宿主；桌面版直接退出，板端自行处理|
|`gb_init(&gb, rom_read, ram_read, ram_write, error, user)`|检查返回错误；`user` 通过 `gb.direct.priv` 获取|
|`gb_get_save_size(&gb)`|取得 cartridge RAM 字节数，再准备/加载存档；此分支没有原核心的 `gb_get_save_size_s` 接口|
|`gb_init_lcd(&gb, draw_line)`|注册 LCD 行回调，参数为核心指针、像素索引、行号|
|`gb_run_frame(&gb)`|更新输入后运行一帧；帧外提交显示/处理宿主任务|

ROM 在 `gb_init` 前必须可读。桌面前端额外检查 ROM 头部表索引与声明长度，
防止候选核心直接索引固定表时越界；移植时不要丢掉这一层输入校验。
具体见 `frontend.c` 的 `gb_init` 前置检查，不支持的特殊 ROM 尺寸编码需另行实现。
RAM read/write 需要做容量边界检查。ROM/RAM 可以由静态区、映射存储或读取缓存提供，
无需照搬桌面 `malloc`；回调读失败不能默默返回随机内存。

### LCD 回调：不能复用原 DMG 调色板下标算法

`gb.cgb.cgbMode != 0` 时，回调像素是 0..63 的调色板索引：

```c
uint16_t rgb555 = gb->cgb.fixPalette[pixels[x] & 63];
unsigned r5 = (rgb555 >> 10) & 31;
unsigned g5 = (rgb555 >> 5) & 31;
unsigned b5 = rgb555 & 31;
uint16_t rgb565 = (r5 << 11) | (((g5 << 1) | (g5 >> 4)) << 5) | b5;
```

`fixPalette` 已把 GB 原始颜色中的 R/B 位序交换为高位 R，不能再交换一次。
RGB565 字节序按面板/DMA 接口处理。DMG 模式仍按 DMG 像素语义转换，
桌面参考使用 `pixels[x] & 3` 输出灰阶。
行数据必须当场转换或复制，不能把核心临时行指针交给异步 DMA 后继续跑核心。
板端可以使用 RGB565 行缓冲，无需桌面版的 160×144×4 ARGB 帧缓冲。

### 输入、复位、RTC、音频

`gb.direct.joypad` 低有效：松开是 `0xFF`，按下时清除对应位。

|键|A|B|Select|Start|Right|Left|Up|Down|
|---|---|---|---|---|---|---|---|---|
|位掩码|01|02|04|08|10|20|40|80|

调用 `gb_reset` 复位；候选根据 ROM 头 `0x143` 的 CGB 位选择模式，
不要用 DMG boot ROM 启动 GBC 专用游戏。当前前端采用核心内置初始寄存器状态。

此候选的 `gb_tick_rtc` 需要宿主每个模拟秒调用一次，与本仓库新版 DMG 核心
内部处理 RTC 的方式不同。参考前端按每帧 70224/4194304 秒累计；
CPU 双速不应让 RTC 或显示速度也翻倍。`gb_set_rtc` 可设置初始 RTC；
电池 RTC 的持久化和关机时长补偿尚未接入。

当前 `ENABLE_SOUND=0`。若开启，需自行提供 `audio_read`/`audio_write` 与
APU 运行、采样、输出及双速时钟适配；只改宏不能得到声音。

## 4. RAM、性能与屏外扩展边界

WSL 当前配置 `sizeof(struct gb_s)=50136` 字节，原 DMG 为 16984 字节，
差值 33152 字节。结构体大小会受 ABI/编译配置影响，应在目标工具链重测。
内部 WRAM 为 32KiB、VRAM 为 16KiB；cartridge RAM、ROM 存储/缓存、显示缓冲、
任务栈和文件系统缓存额外计算。白之书本次 cartridge RAM 为 32KiB。

不要照搬桌面前端的 4MiB ROM 整块加载和 92160 字节 ARGB 帧缓冲当作最低要求。
应在板端测量每帧耗时、缓存命中、栈高水位与 RAM 峰值，不能用 WSL CPU 占比推算。

已增加 `--viewport WIDTHxHEIGHT`，例如 `--viewport 320x240`，由前端调用
原 `gx_viewport.h` 合成器，不改变 PPU/核心。`background.h` 读取 CGB BG map
的 bank0 tile ID、bank1 属性，处理 tile bank、XY flip 和 8 个 BG 调色板。
此通用 VRAM 路径只知道显存内容，不保证未知游戏的正确周边地图。

银版精确指纹匹配后，`viewport.h` 复用共享地图/连接图/对象解码函数，另外处理颜色。
这里 `gx_gb_view.wram` 借用物理 WRAM bank0+1，用于精确银版已验证的固定状态，
不能当成任意 CGB 游戏当前 CPU 的 C000..DFFF 内存视图。
原 DMG 模块语义未修改；没有把其灰阶输出当作 CGB palette index 使用。

### 银版 GBC 的新增地址证据

来源仍是 [Narishma-gb/pokesilver 固定提交](https://github.com/Narishma-gb/pokesilver/tree/edbe53978ef1777fc5c41019e17b7544070eab92)。
只用其反汇编核对布局/算法，不把反汇编仓库变成构建依赖。

- `wTilesetPalettes=$D0C1`：`ram/wram.asm` 中 tileset 结构尾部，结合原后端已验证的
  `$D0B7` block bank 推导，并用真实运行 RAM 核对指针 `$40C7`。
- 调色板表实际在 **ROM bank $02**：`main.asm` 的 `bank2` 包含
  `engine/tilesets/map_palettes.asm` 与 `gfx/tileset_palette_maps.asm`。
  `wram.asm` 原有 “bank 3f” 注释在此日版源码中不适用，不能照抄该注释。
- 每两个 tile ID 共用一个字节，偶数取低 nibble、奇数取高 nibble；bit3 表示
  按 map group 取 palette。固定版本 `MapGroupPalettes` 全部为屋顶 palette 6。
- 每帧对可见 BG map 的 bank1 属性逐格核对计算结果；有不一致则这一帧不扩银版地图，
  避免错误着色。地图识别仍经过共享后端的版本/场景/tilemap 一致性检查。
- 当前 live object 的 palette 来自 `OBJECT_PALETTE`（对象偏移6）的低3位；
  未激活/邻图对象遵循地图 palette override，默认取 sprite descriptor 偏移5。
  当前银版场景 OBJ 图像在 VRAM bank0；没有把此规则用于其他 CGB 游戏。
- 精灵颜色0透明，按 CGB OAM 顺序处理优先级；银版 terrain 属性不带 BG priority bit，
  grass/behind-BG 按非零背景像素遮挡。原生边缘的有限补画见下一节。

### 移动精灵进入/离开原生画面的交接（2026-10-21）

游戏会在原生边界停止/开始发布某些NPC的OAM，但共享对象解码器仍保留其屏外
位置和动作。原先只绘制边缘、再无条件复制核心160×144，就会在没有OAM的那帧
切掉半个NPC；部分精灵看起来闪烁、抖动或擦除后重画。

`silver_sprites.h` 将普通行绘制拆成 `gbc_silver_sprite_layer(..., seams_only)`：

- 屏外依旧用完整对象绘制。中央前景回调只处理 `oam_pose==0`，且实际ROM姿势
  所有8×8部件的包围盒跨越原生四边之一的对象；完全内侧/完全外侧均不补。
- 两侧使用同一份帧快照位置、facing、调色板和ROM图像；已匹配OAM的对象仍以
  核心发布的位置/姿势为准，中央不重复绘制。
- 调色板索引必须在 `gbc_view_begin` 与对象姿势一起捕获，不能在每条扫描线重读
  live对象槽。实走中游戏在可见扫描线期间复用了槽位，旧实现会让同一精灵中途
  换色，甚至因后半帧解析失败而消失。已发布对象以其OAM属性低3位为准；其他
  当前地图对象读取live槽或地图记录。保留的旧地图对象按地图组、地图号、对象
  编号查颜色历史，避免套用新地图槽位；无历史时按所属地图ROM事件解析。
  历史仅保存颜色，不延长对象生命周期，也不缓存旧位置。每帧索引快照及历史
  共占 `5*PGB_SILVER_MAX_OBJECTS` 字节，另有少量控制字段。
- 原生像素索引>=32表示核心已画OBJ，补画不能覆盖它，保护玩家及其他已发布精灵。
  颜色0仍透明；behind-BG分支恢复原生背景，不覆盖下层原生OBJ。
- 这是第二个明确的中央像素例外，仅为跨界缺口补充显示；不改OAM/VRAM/WRAM、
  ROM读取策略、NPC AI或PPU时序。菜单/无效场景、`--terrain-only`、原生尺寸均不补。

`test_sprite_seams.c` 合成四边交接与保护规则；`test_silver_sprite_seams.c` 在真实
移动路线中将“完整live对象绘制”作为边缘参考，检查包括核心缺失的像素，
不能只比较两边都已画出的不透明像素（那样会漏掉闪烁缺口）。
实际路线包含返回，并断言确实遇到逐行槽位变化与旧解析失败的样本，同时验证
这些帧的调色板快照保持不变。合成测试另覆盖四边和跨地图槽位复用。
该实际路线仍要求吉花市group26/map3、x10/y9的确定性测试存档，其他位置明确SKIP。

### 跨图屋顶颜色修正（2026-10-21）

原实现的地形有空间归属，但所有地图的屋顶都引用当前硬件 BG palette 6，
所以进入别的 map group 后，仍可见的旧城屋顶随硬件调色板一起改变。
`silver_roofs.h` 在最终 ARGB 输出阶段修正这个显示问题，不写核心 palette/VRAM/RAM。

- `RoofPals` 固定 ROM 地址 `$B934`，每组8字节，前4字节为白天两种屋顶色，
  后4字节为夜间两种色。用固定反汇编 `gfx/tilesets/roofs.pal` 全216字节序列
  在精确 ROM 中唯一匹配，且 `engine/gfx/color.asm` 的加载指令也指向同一地址。
- 同一加载代码在 ROM `$B639` 附近读取 `wTimeOfDayPal=$D55B`，低2位>=2选夜间。
  例：26组吉花市白天颜色原始 BGR555 为 `$71DF/$54BF`，24组为 `$3BF4/$16EB`。
- 使用与稳定边界地形相同的地图矩形距离及 map ID tie-break 选择屋顶空间归属，
  而不是缓存“上次看到的颜色”；未访问的相邻区域和返回旧地图也有正确颜色来源。
- 仅修正 palette-map 解码后使用屋顶 palette6 的地形 tile，以及该 palette 的颜色1/2；
  包括字面 nibble6 和 bit3 间接映射，不能只认 bit3（吉花市部分屋顶使用字面6）。
  普通 BG、OBJ、菜单、无效场景不改。当前地图屋顶也按空间归属取色，避免地图
  编号已更新、硬件屋顶色仍滞后一帧时闪色。
- 为避免中央/边缘接缝，旧地图屋顶即使还在中央160×144内也作同样的显示修正。
  这是原生逐像素保持规则的明确例外之一；此显示修正本身不写模拟内存。
- 空间归属按8×8地形 tile 缓存，每帧清空，避免对每个屋顶像素重新遍历地图图谱。
- 仅在游戏正常 `BGP=$E4` 阶段替换；渐暗阶段保留硬件输出。不能要求硬件屋顶色
  先匹配当前地图，因为返回旧地图时恰好存在地图编号先更新、颜色后上传的过渡。
  昼夜选择同样在帧开始捕获，避免逐行读取发生半帧切色。

`test_silver_roofs.c` 检查两种颜色、昼夜、26↔24重复切换、渐暗及OBJ/无效场景排除。
`test_silver_roof_route.c` 用实际按键从吉花市走到29号道路并返回，校验旧城矩形内
的屋顶像素，生成 `roof-before.ppm`、`roof-after.ppm`、`roof-return.ppm`。
此实走用例要求 Continue 后在 group26/map3、x10/y9 的测试存档；其他存档明确报
SKIP（退出77），不冒充实走通过。独立昼夜/地图颜色测试仍会运行。

### NPC真实运行范围扩充及预测交接（2026-10-21，后续修正）

此前GBC只画未激活NPC的初始/记忆位置，没有运行范围扩充。它们后来被游戏真正
激活，尤其已经走动时，真实位置与预览不一致，造成旧形象擦除、在新位置重画。
颜色快照和边界缺口测试并不能证明这种跨帧交接正确。

- `frontend.c:read_rom` 在viewport初始化后接 `gbc_view_rom_read`，复用共享后端
  `pgb_silver_jp_rom_read`，配置 `PGB_SILVER_ALL`。核心单头文件和共享GB模块未改。
- 每次ROM读取同步实际PC/BC，WRAM借用物理bank0+1；精确ROM CRC识别仍必需。
  复用地址 `$4397/$439C/$43A9/$43AE`、`$43C3/$43C8/$43D5/$43DA` 的保留范围，
  `$878D/$8792/$879B/$87A0` 的激活范围及 `$9675A` 的整窗扫描入口。
  `$592F`/PC `$5930` 的OAM发布保护排除远处坐标模256回绕；初始朝向记忆读取仍
  限定共享模块的PC `$18D1`。这些都是原有立即数/数据覆盖，不改指令长度或核心时序；
  游戏执行路径、RNG及NPC状态会有意改变，不能再宣称与无扩围游戏状态相同。
- 当前地图已有live槽的NPC由原游戏持续运行，显示读取其真实位置和动作。
  `gbc_silver_remove_predictions` 只保留live对象以及movement类型1、6..9的静止预览；
  未激活/邻图的行走、随机转向及脚本类型不画猜测姿势，防止激活时跳位置。
  邻图尚未进入当前地图时没有运行额外AI，也不制造独立随机模拟。
- 输出支持任意已验证容量的尺寸，但游戏只有13个live槽（含玩家），共享策略的
  pad按每轴最多31个16像素逻辑格计算；不能据此承诺无限范围NPC运行。
  原生160×144或 `--no-background` 不扩围。`--terrain-only`只关额外绘制，保留AI扩围。
- `test_silver_viewport.c` 的8000帧原版对照明确使用原ROM读取，隔离展示本身影响。
  生产ROM-read路径由实走 `test_silver_sprite_seams.c` 覆盖：检查屏外live持续移动、
  OAM交接及连续帧世界位置（本路线每帧不超过4像素），并排除移动对象的猜测预览。
  `test_sprite_seams.c` 对37种movement逐一验证预览筛选。多尺寸游戏截图不再与
  无扩围模式作一致性断言；关闭背景扩充的模式仍与原生截图比较。

### 切图短暂停顿排查（2026-10-21）

WSL、优化构建、320×240、吉花市↔29号道路的4000帧确定性输入，逐帧记录核心+
LCD回调绘制耗时、地图状态和SCX/SCY：行走中位1.018ms，切图窗口最高1.938ms，
没有超过70224/4194304秒（约16.743ms）的帧预算。此测量不含SDL窗口呈现与等待。
在同一接近边界的游戏历史下，第2580帧恢复原ROM读取，两个切图仍发生在2624、
2937帧。2600..2999帧地图号、玩家坐标、SCX/SCY、地图状态及LCD行数全部一致。
两种策略都在2625→2630、2938→2943帧间保持相同SCX：正常每2帧滚动一次，切图
间隔变为5帧，多停3帧，约50ms。该路线的轻微顿挫来自游戏切图流程，未测到NPC
扩围引入额外滚动停顿；不能由此排除窗口呈现、宿主调度或其他地图的性能问题。

### 切图停顿处理：主循环接入（2026-10-21）

`frame_runner.h` 的 `gbc_run_display_frame` 替换前端每次显示前的 `gb_run_frame`。
首次看到已知connection目标的MAPSTATUS_ENTER时仅记下目标，保留这一帧。
同时要求 `hMapEntryMethod=$FFA1` 为 `$F7`（connection）；精确ROM `$96696` 的
`F0 A1 FE F7` 与原EnterMap中的比较对应，避免把门/warp误判为连接图。
下一次显示前继续执行原CPU/PPU帧，直到MAPSTATUS_HANDLE、新地图对象历史及完整LCD行
同时就绪，避免呈现半更新地图。每次最多额外6帧，并在主循环处理时间达到8ms后
停止追加（单个模拟帧本身不可抢占）。低性能宿主可退回原加载节奏。

不跳过指令、不改地图加载脚本、NPC状态或PPU时序；只压缩加载帧的显示等待。
warp/门/菜单和不支持的ROM、原生尺寸、关闭背景扩充不触发。RTC继续按显示帧
对应的真实时间推进，不能对追赶帧重复计时。输入在本次显示期间保持同一份快照。
移植时传入单调毫秒时钟以启用时间预算；NULL仅用于确定性测试，仍有6帧硬上限。

`test_transition_catchup.c` 对实际往返做双实例检查：参考逐一执行全部原帧并使用
相同输入，逐显示帧比较CPU、WRAM、VRAM、OAM、IO、CGB调色板、存档RAM及完整
320×240输出。两次切图额外运行共6帧，镜头更新间隔从5个原帧变为2个显示帧。
另验证菜单不能继承追赶标志。优化构建含时钟预算的实际测量，切图窗口最高约
4.77ms，低于16.743ms帧预算；这不含SDL呈现与宿主调度。

### 扩屏缓冲预算

前端分配 `WIDTH*HEIGHT*4` 字节 ARGB 输出和 `WIDTH` 字节索引行缓冲；
目前另保留 92160 字节原生 ARGB 缓冲（便于原生对照），与核心、银版 adapter、ROM/RAM
分开计算。320×240 输出为 307200 字节，640×480 为 1228800 字节。
板端可改为逐行 RGB565 输出，不能直接照搬这些桌面缓冲。
共享精灵排序与新 CGB 精灵行绘制有 `MAX_OBJECTS*4` 个 part 的局部数组，
默认约12KiB栈，移植时须预算或把缓冲放到宿主静态区/降低对象容量。
`--terrain-only` 只减少工作，不减少编译时结构体/容量保留。

## 5. 移植后的验证入口

桌面参考构建：`python3 extensions/tools/build_peanut.py --gbc`。
完整现有验证：`python3 extensions/gbc/verify.py 'ROM副本或只读输入路径'`。
银版扩屏回归：`python3 extensions/gbc/verify_silver_viewport.py ROM SAVE`。
输出固定在 `build-extensions/bin/peanut-sdl-gbc`；测试数据位于
`build-extensions/gbc-verification/`，不回写输入 ROM。

已验证与未验证项目、真实 ROM 指纹和测量环境见 [README.md](README.md)。
当前通过的是冒烟和部分回归；声音、游戏内存档后重启继续、完整 CGB 时序仍未认证。
