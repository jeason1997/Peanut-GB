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
GBC 前端自动按 ROM 头选择 CGB/DMG；不加载 DMG boot ROM。尚未接声音，
尚未接 CGB 屏外画面或白之书地图后端，只有原生 160×144。

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
