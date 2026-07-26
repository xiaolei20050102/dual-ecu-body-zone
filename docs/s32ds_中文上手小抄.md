# S32 Design Studio 中文上手小抄

这份小抄只讲 **S32 Design Studio for ARM 2.2** 这个编辑器怎么操作，不讲 S32K144 芯片原理。

## 先记住：现在只会用到 5 个动作

| 你想做什么 | 英文入口 | 作用 |
| --- | --- | --- |
| 新建工程 | `S32DS Application Project` | 创建一个新的代码工程 |
| 看工程文件 | `Project Explorer` | 左上角，查看 `main.c`、工程配置等 |
| 写代码 | `main.c` | 中间编辑区域，程序入口文件 |
| 编译 | `Build (All)` 或 `Project > Build All` | 只检查代码能否生成程序文件，不需要开发板 |
| 下载/调试 | `Debug` | 开发板插到电脑后才用 |

## 主界面每块区域是干什么的

```text
左上：Project Explorer   工程目录和文件
中间：代码编辑区          打开 main.c 后在这里写 C 代码
左下：Dashboard           新建、编译、调试的快捷入口
底部：Problems / Console  报错列表和编译输出
右上：Outline             当前 C 文件里的函数、变量目录
```

不小心把某个窗口关掉：

```text
Window > Show View
```

在这里重新打开 `Project Explorer`、`Console` 或 `Problems`。

## 工程里常见内容

你现在创建的工程名是 `s32k144_bringup`。

| 名称 | 实际意思 | 现在是否要改 |
| --- | --- | --- |
| `Sources/main.c` | 你自己写业务代码的主要入口 | 后面会改 |
| `Components` | 图形化配置的时钟、引脚、外设 | 没有明确目标时不要乱改 |
| `pin_mux` | 引脚复用配置，相当于 STM32CubeMX 的引脚页 | 做 LED/CAN/串口时才改 |
| `clockMan1` | 时钟配置 | 暂时不动 |
| `intMan1` | 中断配置 | 暂时不动 |
| `Debug_FLASH` | 当前工程的调试/烧录配置 | 板子到后才用 |

## 最常用的编译流程

1. 在 `Project Explorer` 中单击工程 `s32k144_bringup: Debug_FLASH`。
2. 点击左下 `Dashboard > Build/Debug > Build (All)`。
3. 看底部 `Console`：
   - 出现 `Build Finished`、没有红色错误：编译成功。
   - 出现 `error:`：编译失败；先看第一条红色错误，不要一次看完所有报错。

注意：`Build` 只是在电脑上编译；没有开发板也能做。

## `main.c` 应该在哪里写代码

`main.c` 里会有很多自动生成的注释。以后只在这两块中间写自己的代码：

```c
/* Write your local variable definition here */

/* Write your code here */
```

不要删除带有 `DON'T REMOVE THIS CODE` 的内容；那是工具自动生成和维护的代码。

## 现在先不要碰的按钮

| 按钮/页面 | 为什么先不碰 |
| --- | --- |
| `Debug` | 需要连接 S32K144 开发板 |
| 顶部 `Run` | 没接板子时没有实际运行目标 |
| `pin_mux` 的 ADC/CAN/GPIO 表 | 不知道开发板接线前改了也无法验证 |
| `S32DS Extensions and Updates` | 当前安装已经可用，不要混装旧更新包 |

## 以后板子到手后的最短操作路线

```text
打开工程
→ 配置 LED 对应的 pin_mux
→ 在 main.c 写闪烁代码
→ Build (All)
→ 连接开发板
→ Debug 下载并观察 LED
```

## 看不懂英文时的处理方式

先认动作，不必逐句翻译：

| 英文 | 中文 |
| --- | --- |
| New | 新建 |
| Open | 打开 |
| Save | 保存 |
| Build | 编译 |
| Clean | 清理旧编译结果 |
| Debug | 下载并调试 |
| Resume | 继续运行 |
| Terminate | 停止调试 |
| Error | 错误 |
| Warning | 警告，通常不阻止编译 |

遇到一个新的窗口或报错，截屏发来即可；只需要理解当下正在用的那一项。
