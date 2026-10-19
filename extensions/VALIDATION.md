# 本次模块化验证记录

环境：WSL/Linux，gcc -O2；真实 ROM 为精确日版银Rev1。ROM和用户存档仅只读；SDL使用 /tmp 存档副本与隐藏1:1窗口。没有在Windows编译或运行。

|验证|结果|
|---|---|
|独立合成器|GB/NES尺寸、奇数边距、160×145、输入行顺序、边界保护、前景开关、只处理新增像素通过|
|不包含Peanut头的独立游戏后端|默认容量和28对象/4地图/无缓存/4灰阶编译运行通过|
|真实ROM经中立状态视图+原生LCD回调|默认及低容量均1326有效场景、265200行；未用核心viewport API|
|原有完整8000帧|7952连贯展示帧，CPU/PPU/卡带RAM与相同NPC策略基线一致；1357原屏重建逐像素一致|
|Cherrygrove→Route29实走|251823稳定填充采样；邻图Teacher预览/接管100/30帧，无空洞重复|
|功能开关|全关逐字节ROM读取等同原ROM、terrain-only、关闭邻图NPC、对象满时有界截断、重配失效缓存通过|
|SDL CLI|掩码0/1/49/127及28对象接受；非法依赖64在创建窗口前拒绝|
|ASAN+UBSAN|当前存档focused回归通过|
|干净基线Patch|模块/接入/回归三补丁均apply --check与实际apply通过，源文件SHA256一致|
|原核心回归|CPU指令、指令时序、dmg-acid2、200305项viewport检查，单头/共享对象两构建通过|

小容量测试确认可运行与边界安全，不承诺被容量截断的远处地图/NPC仍完整。未验证第二款游戏、第二种模拟器实现、真实NES后端或目标MCU性能。

性能数字见PORTING.md：是此前实验阶段的WSL静止场景数据，本次没有把结构重构描述为新的性能提升。

## 工作区收尾

主目录所有已跟踪文件已恢复为HEAD字节，原实验新增头与测试源已从examples/test撤出，封装于可选补丁。此前完整实验源码与行尾备份在忽略目录 build-viewport-validation/pre-cleanup-source/。
Git最终应仅显示 `?? extensions/`，没有主核心/examples/test的修改或暂存内容。验证接入时使用临时干净检出，不把Patch再应用回主工作区。

## 直接构建入口验证

`python3 extensions/tools/build_peanut.py --with-tests` 已在WSL完成真实构建；固定输出 build-extensions/bin/peanut-sdl 与 test_silver_jp。SDL二进制以dummy视频/音频驱动运行200×150共5帧成功，完整focused跨地图/NPC回归通过。主目录原版源码未改变。
构建隔离目录会初始化独立Git边界，防止嵌套目录运行git apply时跳过路径；脚本失败会返回非零状态。
导出工具允许提交后的分支HEAD，只固定集成补丁的上游基线。
