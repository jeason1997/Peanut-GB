# V3s Linux fbdev 运行记录与复现

本前端独立于 SDL/Pico SDK；不修改模拟核心。`frontend_fb.c` 使用既有
`viewport.h`、`frame_runner.h`，保留 NPC 范围、精灵/屋顶颜色快照及 8ms/6帧切图预算。

## 构建

Linux 原生构建（也可在板上执行）：

```sh
sh extensions/tools/build_gbc_fb.sh
```

WSL 交叉静态构建（当前设备是 Alpine musl；不要使用现有 uClibc 动态工具链）：

```sh
zig cc -target arm-linux-musleabihf -mcpu=cortex_a7 -O3 -std=c99 -static \
  extensions/gbc/frontend_fb.c -o build-extensions/bin/peanut-fb-gbc-arm
```

`CC`、`CFLAGS`、`LDFLAGS` 可传给构建脚本。产物放忽略的 build-extensions，勿提交。
格式转换独立验证：

```sh
cc -O1 -g -fsanitize=address,undefined -std=c99 extensions/gbc/test_fb.c -o /tmp/test-fb
/tmp/test-fb
```

## 设备与传输

2026-10-21 实查：COM15，115200/8N1，已有 root shell；Alpine 3.24.1，
Linux 7.2.4 armv7l，单核 Cortex-A7，CPU 当前 1008000 kHz，物理内存约48 MiB。
根文件系统为 TF ext4；/tmp 为约24 MiB tmpfs。设备有 GCC 15.2.0。
当时 Wi-Fi 未连接，无 IPv4 地址，无 USB UDC/usb0，也没有 /dev/input 事件设备。
因此复用串口，不改网络、启动配置或固件。

PowerShell 工具只适用于已经登录的 shell；参数来自本设备实查，不是所有板的默认值：

```powershell
powershell -NoProfile -File extensions/tools/v3s_serial.ps1 -Port COM15 -Baud 115200 -Send 'uname -a'
# 先建独立目录，再传已准备的 tar.gz，比较本地和设备 SHA256：
powershell -NoProfile -File extensions/tools/v3s_serial.ps1 -Send 'mkdir -p /tmp/peanut-gbc-20261021'
powershell -NoProfile -File extensions/tools/v3s_serial.ps1 -Upload build-extensions/v3s/deploy.tar.gz -Destination /tmp/peanut-gbc-20261021/deploy.tar.gz
```

传输包需包含 extensions/gbc 的运行时头文件及 frontend_fb.c、extensions/include、
extensions/games、extensions/ports、ROM副本和独立存档副本。测试期间不写存档。
原 ROM CRC32=0aea5383、SHA1=a11d5ddc26eb826086593f82370b15d16404d33e；
源文件和 build-extensions/silver-gbc-play.sav 必须保护。

## 运行与控制

设备目录 `/tmp/peanut-gbc-20261021` 内：

```sh
gcc -O3 -std=c99 extensions/gbc/frontend_fb.c -o peanut-fb-gbc
./peanut-fb-gbc silver.gbc --save test.sav --frames 3200 --input route.txt --profile
# 交互式，无帧数上限：
./peanut-fb-gbc silver.gbc --save test.sav --profile
```

串口 WASD=方向，Z/X=A/B，E=Start，R=Select，Q=退出。每个字符保持8显示帧，
连续发送可继续按住；没有硬件按键设备时以此控制。输入脚本与 SDL 相同，
每行 `显示帧 hex_pressed_mask`，A=01/B=02/Select=04/Start=08/右10/左20/上40/下80，
保持到下一行。`route.txt` 复用 test_transition_catchup.c 的精确往返路线，部署时从
`extensions/gbc/inputs/silver-connection-route.txt` 复制并改名。
`--snapshot final.ppm` 导出最终 ARGB；默认永不写 cartridge RAM 文件。

前端运行时将 tty1 临时切至 KD_GRAPHICS，退出恢复原模式；FBIOBLANK 唤醒屏幕。
查询 FBIOGET_VSCREENINFO/FSCREENINFO 后，按实际 bpp、RGB位域、stride、x/yoffset
校验和写入 mmap；支持16/32bpp真彩色，屏幕分辨率须与 viewport 一致，默认320×240。
不主动改变显示模式。ARGB整屏307200字节、RGB565暂存153600字节，加320字节索引行，
没有桌面版额外的92160字节原生ARGB缓冲。RGB565有与通用位域转换一致的快速路径。

计时使用 CLOCK_MONOTONIC。RTC按实际墙钟秒推进，不随追赶帧或运行掉帧加速/减速；
RTC仍不持久化。`--fast` 取消59.7FPS限速。run_avg_ms含draw_avg_ms，
两者相减是模拟/核心等工作；present_avg_ms含格式转换和mmap复制。
fb_convert_avg_ms/fb_copy_flush_avg_ms分别报告这两段。mmap复制耗时不包括异步SPI传输、
面板扫描或肉眼端到端延迟，输出FPS不能宣称面板完成刷新率。没有音频。
## 2026-10-21 实测结果与刷新修正

实测 ioctl：panel-mipi-dbid，320×240，virtual320×240，offset0,0，RGB565
R=11/5 G=5/6 B=0/5，stride640，smem153600，PACKED_PIXELS/TRUECOLOR。
ARM ABI 核心50080字节、adapter17928字节；静态ARM/musl可执行文件约110KiB，
进程maxrss1792KiB。输出与RGB565暂存不代表全部系统/驱动内存。

首次 mmap 版本虽然模拟/提交59.7FPS，用户观察屏幕明显卡顿。进一步检查当前
Linux 7.2.4 源码 `drivers/gpu/drm/drm_fbdev_shmem.c:184`，其 mmap deferred_io
默认 `HZ/20`（本机CONFIG_HZ=100，即50ms）。这会把多次用户态写入合并成一次刷新。
`drivers/video/fbdev/core/fb_defio.c:266` 的 fsync 调用 flush_delayed_work，
可以在用户态推动脏页处理，不必刷内核。新前端默认在每帧 mmap 复制后 fsync(fb_fd)，
`--deferred-fb` 关闭此行为用于对照；fsync仍不是物理面板扫描完成的栅栏。

使用 spi0.0 sysfs statistics 在游戏地图场景采样，两个10秒窗口：

|路径|窗口秒|TX字节增量|整屏大传输增量|SPI消息增量|整屏传输率|
|---|---:|---:|---:|---:|---:|
|仅mmap|10.01|22580817|147|882|14.69/s|
|mmap+fsync|10.04|91705767|597|3582|59.46/s|

本机每次整屏事务含153600字节像素+11字节命令，6条SPI消息和1条>=65536字节传输；
两项独立计数与TX字节增量完全吻合。该测试证明SPI整屏提交吞吐，不证明每帧画面
完整无撕裂地被面板扫描；没有外部相机或TE信号测量，不宣称测到了面板光学FPS。

主动flush版本，1800显示帧/30.137秒，模拟59.7FPS，地图1395帧：
平均run7.031ms（含draw4.044ms，模拟等其余约2.987ms），present4.081ms，
地图工作峰值24.810ms，5帧超16.743ms预算。全场景转换均值2.304ms，
FB复制+flush1.843ms。进程CPU17.497秒（不包含所有内核工作线程CPU）。
声音未实现。串口WASD/Z/X/E/R八个实时按键均记录到实际joypad输入。

首次未flush的3200帧往返已完成26/3→24/3→26/3，最终玩家25,7，0核心错误、
0调色板不匹配，380322个屏外精灵像素。8ms切图预算只容纳每次1个额外帧，
总extra=2、completed=0，不能宣称达到桌面6额外帧全部加载完成的切图效果。
`completed=0`指未在单次追加预算中完成全部加载；实际两次地图连接均已通过。

复现往返脚本保存在 `inputs/silver-connection-route.txt`。它依赖输入存档继续后
吉花市26/3、x10/y9；其他存档不能据此作同一路线的性能或正确性比较。
部署包在临时目录使用silver.gbc和test.sav；板端test.sav运行前后SHA256均为
27ad71d44f8fca5e6a616006ce866db36921251710d0de48e81e580c59febf71，未写存档。
主动flush版最终3200帧路线复测：53.577秒，59.7模拟FPS，进程CPU32.572秒；
地图2795帧，run均值7.264ms（draw4.282ms，其余2.982ms），present4.271ms，
合计11.535ms；地图峰值20.879ms，6帧超过16.743ms预算。
全场景转换2.339ms，复制+flush1.954ms。两次地图切换完成、最终25,7，
379710个屏外精灵像素，0核心错误、0调色板不匹配；本次8ms追加预算无余量，
extra=0。游戏原始切图加载节奏因此仍可能短暂停顿，不宣称板端已消除这一点。
不能为了追平桌面结果擅自突破原本8ms预算。

设备运行期间用 `dd if=/dev/fb0 of=screen-flush.rgb565 bs=153600 count=1`
读回实际fb内存，解码截图确认320×240扩展地图输出；这不同于外部拍摄面板。
当前主机截图和日志集中在忽略的 build-extensions/v3s/。

SPI采样的可重复设备命令（运行期间，应用stdout重定向以避免终端刷新干扰）：

```sh
./peanut-flush silver.gbc --save test.sav --frames 1800 --input route.txt --profile </dev/null > flush.log &
app=$!
sleep 10
cat /proc/uptime /sys/bus/spi/devices/spi0.0/statistics/bytes_tx /sys/bus/spi/devices/spi0.0/statistics/messages /sys/bus/spi/devices/spi0.0/statistics/transfer_bytes_histo_65536+
sleep 10
cat /proc/uptime /sys/bus/spi/devices/spi0.0/statistics/bytes_tx /sys/bus/spi/devices/spi0.0/statistics/messages /sys/bus/spi/devices/spi0.0/statistics/transfer_bytes_histo_65536+
wait "$app"
cat flush.log
```

前后大传输计数差除以uptime差，得到本设备整屏SPI传输率；同时检查
TX差=153611×传输数、messages差=6×传输数。加入`--deferred-fb`重复即为旧路径。
这些关系是本机实测，不应用到其他SPI分块/格式配置。

2026-10-21：按用户要求撤回后续双缓冲、DRM同步提交和固定节奏版本；
当前设备恢复 peanut-flush，保留 fbdev 每帧 fsync 修正及 PC 串口工具。

PC图形串口工具启动和操作见 [V3S_CONTROL.md](../tools/V3S_CONTROL.md)。
