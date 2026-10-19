# 给下一位 AI 的接手入口

先读 README.md 与 PORTING.md，再改代码。

**整理后的主仓库只增加 extensions/。不要直接修改根目录核心、examples/、test/ 来接实验。**
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
