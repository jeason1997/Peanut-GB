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
|`extensions/tools/build_peanut.py`|新增 `--gbc` 分支，单独编译前端|板端可不用|
|`extensions/gbc/test_core.c`|银行、颜色、GDMA、KEY1、reset 冒烟测试|验证时参考|
|`extensions/gbc/verify.py`|隔离运行 sanitizers、原 DMG fixtures、真实 ROM 输入及文件往返|桌面验证用|
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

当前 GBC 输出只有原生 160×144；没有 `--viewport 320x240`。
通用 `gx_viewport.h` 可复用，但已有 DMG VRAM 后端和《银》地图后端不能直接接 CGB；
颜色、VRAM bank、属性、优先级及游戏地图都需要相应接线/适配。

## 5. 移植后的验证入口

桌面参考构建：`python3 extensions/tools/build_peanut.py --gbc`。
完整现有验证：`python3 extensions/gbc/verify.py 'ROM副本或只读输入路径'`。
输出固定在 `build-extensions/bin/peanut-sdl-gbc`；测试数据位于
`build-extensions/gbc-verification/`，不回写输入 ROM。

已验证与未验证项目、真实 ROM 指纹和测量环境见 [README.md](README.md)。
当前通过的是冒烟和部分回归；声音、游戏内存档后重启继续、完整 CGB 时序仍未认证。
