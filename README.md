# 双ECU车身区域控制诊断刷写实验平台

当前目标：先完成不依赖硬件的工程准备，等板子到后再进入 Blink、UART、FreeRTOS、CAN 验证。

## 当前阶段

Week 1：环境、仓库、基础通信准备。

今天只做三件事：

1. 项目目录骨架
2. 环境版本记录
3. PC 端 CAN/DBC 工具准备

## 目录说明

- `firmware_s32k/`：S32K 主控 ECU 固件
- `firmware_f411/`：STM32F411 执行器 ECU 固件
- `tools_pc/`：PC 端调试、诊断、刷写工具
- `config/`：DBC、LIN 矩阵、DID/DTC 表
- `docs/`：需求、架构、接口、测试、演示文档
- `hardware/`：BOM、接线、照片、测量记录

## 暂不做

- UDS
- Bootloader
- 电机闭环
- 复杂 GUI

