# mtop

[English](./README.md) | [简体中文](./README.zh-CN.md)

`mtop` 是一个面向 Apple Silicon Mac 的终端监控工具。

它希望让熟悉 `htop` 和 `nvtop` 的用户上手时没有门槛，但它的设计重点放在 macOS 和 M 系列 SoC 的真实特点上：统一内存、异构核心、GPU 活动、以及对本地 AI 工作负载有意义的功耗信息。

## 快速开始

直接通过 Homebrew tap 安装：

```bash
brew install lxrzlyr/mtop/mtop
```

或者先 tap 一次，再使用短名字：

```bash
brew tap lxrzlyr/mtop
brew install mtop
```

运行：

```bash
mtop
```

预览模式：

```bash
mtop --demo
```

## 截图

![mtop demo](./docs/assets/mtop-demo.png)

## 为什么要做这个工具

因为直到今天，macOS 上依然没有一个真正让人称手、又足够理解 Apple Silicon 的终端监控工具。

过去监控 Mac 的资源使用情况，尤其对个人用户来说，似乎没有那么重要。AI 时代来临之后，这件事变了。越来越多的人开始在本地跑推理、编译更重的栈、调模型、压测统一内存、观察机器到底把算力和功耗花在了哪里。

我们希望能为每个用户都提供一把顺手的工具。

`mtop` 想解决的是：

- 让 Apple Silicon 的核心类型不再被当成“普通 CPU”糊弄过去
- 不用 `sudo` 也能拿到足够有用的信息
- 需要更深指标时，再让 root 模式补充增强
- 把性能、内存压力、GPU 活动和进程行为用一个舒服的 TUI 展示出来

## mtop 是什么

`mtop` 是：

- 一个 macOS-first 的 Apple Silicon 终端监控工具
- 一个针对 M 系列 SoC 优化的 TUI
- 在进程交互逻辑上参考了 `htop`
- 在 GPU 图形展示上参考了 `nvtop`
- 使用 C++20 与 ncurses 从头实现

## 它能显示什么

普通权限下：

- SoC 型号与 Apple Silicon 核心拓扑
- 按核心类别分组的 CPU 利用率
- 统一内存与 swap 使用情况
- 支持排序、过滤、树形、搜索、选择、nice、signal 的进程主表
- GPU 利用率
- 电池、启动时长、系统负载

root 增强模式下：

- thermal pressure
- ANE 活动（当平台可见时）
- 来自 `powermetrics` 的估算 SoC 子系统功耗
- GPU 功率 / 频率增强信息
- 基于 `powermetrics` 推导的进程核心类别混合信息

## 关于 `SOC` 功耗

GPU 面板中的 `SOC` 并不是充电功率，也不是整机墙上取电功率。

它是由 `powermetrics` 中 CPU、GPU、ANE 等字段组合出来的 **估算 SoC 子系统功耗**。在 macOS 上，这是当前比较实际、也比较稳定的一条实时功耗数据来源。

所以它的特点是：

- 有参考价值
- 是估算值
- 反映的是芯片子系统活动，不是电源适配器输入功率

## 当前功能

- Apple Silicon-aware CPU 面板
- `htop` 风格的主进程表
- 增量搜索和过滤
- 树形模式与展开 / 折叠
- 键盘与鼠标排序
- 参考 `nvtop` 的 GPU 曲线面板
- 统一内存与 swap 可视化
- 默认非 root 模式
- root 增强采样路径
- demo 模式
- `cpack` 打包发布

## 构建

```bash
cmake -S . -B build
cmake --build build -j4
```

## 运行

普通模式：

```bash
./build/mtop
```

root 增强模式：

```bash
sudo ./build/mtop
```

UI 预览模式：

```bash
./build/mtop --demo
```

辅助脚本：

```bash
./scripts/preview_demo.sh
```

## 操作说明

主界面交互：

- `Up / Down / PgUp / PgDn / Home / End`：移动选中
- `F3` 或 `/`：增量搜索
- `F4` 或 `\`：增量过滤
- `F5` 或 `t`：树形模式
- `+ / - / *`：展开 / 折叠 / 切换树节点
- `F6` 或 `>` 或 `.`：排序菜单
- `N / P / M / T / A / I`：PID / CPU / MEM / TIME / NAME / 反转排序
- `F7 / F8` 或 `] / [`：调整 nice
- `F9` 或 `k`：发送信号
- `F10` 或 `q`：退出
- 鼠标：点击表头排序，点击进程行选中，点击函数栏按钮触发动作

## 配置

可选配置文件路径：

```text
~/.config/mtop/config
```

示例文件：

```text
.config.example
```

支持的配置项：

```text
theme=apple
refresh_ms=1000
process_limit=12
demo_mode=false
```

CLI 覆盖：

```bash
./build/mtop --demo
./build/mtop --refresh-ms 500
./build/mtop --theme mono
./build/mtop --config /path/to/config
```

## 安装与打包

本地安装：

```bash
cmake --install build --prefix /usr/local
```

发布打包：

```bash
./scripts/release.sh
```

它会：

- 配置工程
- 编译
- 跑测试
- 安装到 `dist/install`
- 用 `cpack` 生成 `.tar.gz`

Homebrew formula stub 在：

```text
packaging/homebrew/mtop.rb
```

## GitHub Actions

仓库已经包含 GitHub Actions 工作流，会：

- 在 macOS 上构建
- 运行测试
- 安装到 staging 目录
- 使用 `cpack` 生成发布包
- 上传 artifacts
- 对版本 tag 自动附加 Release 资产

工作流文件：

```text
.github/workflows/build-release.yml
```

推荐发布流程：

1. 正常 push 代码，用 workflow artifacts 检查打包结果。
2. 创建版本 tag，例如 `v1.0.0`。
3. push 这个 tag。
4. GitHub Actions 会自动构建并把包挂到 Release 上。

## Homebrew Tap 的关系

`mtop` 和 `homebrew-mtop` 应该保持为两个独立仓库。

- `mtop` 是源码仓库
- `homebrew-mtop` 是 Homebrew tap 仓库

这样做是有意为之：

- 有些用户会手动从源码构建
- 有些用户只想通过 Homebrew 安装
- tap 仓库只维护 formula，不需要承载完整源码历史

主仓库里已经包含了公式生成脚本和 tap 同步工作流：

- [scripts/generate_homebrew_formula.sh](./scripts/generate_homebrew_formula.sh)
- [.github/workflows/update-homebrew-tap.yml](./.github/workflows/update-homebrew-tap.yml)

如果你希望在打 tag 后自动更新 tap，需要在 `mtop` 仓库中配置这个 secret：

- `HOMEBREW_TAP_SSH_KEY`

这个 key 需要对 `lxrzlyr/homebrew-mtop` 有写权限。

## 项目结构

```text
include/     公共头文件
src/         C++ / Objective-C++ 实现
docs/        设计与平台说明
tests/       测试与测试计划
packaging/   打包相关元数据
scripts/     辅助脚本
```

## 未来功能

计划中的方向：

- 更丰富的 setup 面板
- 更完整的树形进程可视化和标记机制
- 更清晰的 root telemetry 展示
- 更好的功耗拆分说明
- 可选的 compact / dense 模式
- 导出 / snapshot 支持
- 在 macOS 能提供足够数据的前提下，增强进程级 GPU 归因

也欢迎提建议。最有价值的反馈通常包括：

- 你在做什么工作负载
- 你希望看到什么信息
- 哪些地方显得吵、误导、或者缺失

## 致谢

`mtop` 从许多优秀工具身上学到了很多。

特别感谢：

- [`htop`](https://github.com/htop-dev/htop) 提供的进程交互思路、控制逻辑、内存展示方式，以及多年积累的优秀 TUI 设计
- [`nvtop`](https://github.com/Syllo/nvtop) 提供的 GPU 图形展示思路和曲线行为参考

它们对这个项目帮助很大。

## License

`mtop` 使用 GNU GPL v3.0 或更高版本。

详见 [LICENSE](./LICENSE)。
