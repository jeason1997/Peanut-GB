# 可分发源码补丁

本目录实际保存可选集成 `0002-peanut-integration.patch`、回归 `0003-regression-tests.patch` 和预期应用结果的哈希；主工程不保持已应用状态。
独立模块补丁0001在导出时从 extensions/ 生成，避免把自己递归打包。

运行 `python3 extensions/tools/export_patches.py --output /tmp/gx-package`。

生成：

1. `0001-extension-module.patch`：独立 extensions 模块、端口、文档与测试工具。
2. `0002-peanut-integration.patch`：可选 Peanut 核心 viewport API、SDL接线、旧兼容头与Make依赖。
3. `0003-regression-tests.patch`：可选 Peanut专用扩展回归。
4. `manifest.json`：基线提交、文件SHA256、补丁SHA256。
5. `APPLY.md`：应用/回退与验证方法。

已修改的本工作目录不应重复应用；对精确基线的干净检出按序应用。
其他模拟器只需独立模块，不应应用Peanut集成补丁。diff里的游戏常量属于游戏后端，不会注入未知模拟器核心。
生成文件不带游戏ROM、存档、任何MCU SDK、编译输出或屏幕截图。
补丁使用LF，现有Windows检出可按APPLY.md的空白处理选项验证，不能用覆盖源码方式绕过冲突。
