# 双 ECU 车身区域控制诊断刷写实验平台

> 面向嵌入式学习与作品集展示的双 ECU 车身执行器平台。项目用一条可逐步验证的链路，串起 CAN、LIN、FreeRTOS、执行器控制、诊断与刷写。

| 项目状态 | 当前阶段 | 核心硬件 |
| --- | --- | --- |
| 进行中 | W2-L1-02：编码器位置读取硬件验证 | S32K144 + STM32F411 + TB6612FNG + N20 编码器电机 |

**实施范围以 [V2.2 项目基线](docs/project_baseline_v2.2.md) 为准，实时完成状态以 [进度计划](docs/v2.2_progress_plan.md) 为准。**

---

## 这个项目要做什么

构建一个“主 ECU 负责整车通信与系统管理、执行器 ECU 负责本地安全控制”的最小车身区域控制系统：

```mermaid
flowchart LR
    PC["PC 工具<br/>诊断 / 抓帧 / 刷写"]
    S32["S32K144 主 ECU<br/>CAN · LIN Master · 网关 · 诊断"]
    F411["STM32F411 执行器 ECU<br/>FreeRTOS · 本地安全"]
    Motor["TB6612FNG + N20 电机<br/>编码器 + 双限位"]

    PC -->|"Classic CAN"| S32
    S32 -->|"LIN"| F411
    F411 -->|"PWM / 方向 / 使能"| Motor
    Motor -->|"编码器 / 限位"| F411
```

| 节点 | 主要职责 |
| --- | --- |
| PC 工具 | CAN 报文观察、诊断请求、故障注入、刷写流程验证。 |
| S32K144 主 ECU | CAN、LIN Master、CAN-LIN 网关、系统状态、UDS/DTC/NvM 与 Bootloader。 |
| STM32F411 执行器 ECU | 电机、编码器、限位、本地超时停车与执行器状态机。 |

---

## 当前进度

### 已完成并验证

- PC 与 S32K144 的 Classic CAN 双向通信已验证，使用 500 kbit/s 作为基础链路。
- STM32F411 正式工程的时钟、TIM1 PWM、TIM2 编码器、USART1、USART2 LIN、FreeRTOS 与 HAL 时基配置已冻结并通过编译。
- TB6612FNG + N20 电机已完成独立驱动封装及实物验证：正转、停止、反转可由 `Motor_*` 接口控制。
- 电机驱动采用端口层解耦：应用层不直接操作 GPIO/TIM，安全状态统一收敛为 PWM=0、驱动失能。
- 编码器位置读取模块已完成源代码和工程集成，并通过 Keil 编译；尚待实物方向/计数验证。

### 正在进行

**W2-L1-02：编码器位置快照的硬件验证。**

当前只确认最小闭环：转动电机时，TIM2 的 AB 相计数能让累计位置按方向增减，并可在 Keil Watch 或 USART1 中观察。此阶段不做速度计算、零位标定、限位联动或 LIN 控制。

### 后续路线

1. 限位输入与本地安全停车。
2. 将电机、编码器、限位组织为 F411 执行器状态机，并补齐 FreeRTOS 周期任务监测。
3. 接入 LIN 从节点，完成 S32K144 CAN-LIN 网关的最小命令/状态闭环。
4. 逐步加入 UDS、DTC、NvM、Bootloader 与 PC 自动化验证。

---

## F411 当前软件结构

电机模块已经按“业务策略与 STM32 HAL 分离”的方式组织：

```text
ControlTask
    │
    ├── motor_driver       状态、参数校验与安全顺序
    │       │
    │       └── motor_port 端口抽象
    │               │
    │               └── motor_port_stm32  TIM1 / GPIO / HAL
    │
    └── encoder_driver     累计位置与计数跨界处理
            │
            └── encoder_port_stm32  TIM2 Encoder Mode / HAL
```

| 模块 | 当前职责 |
| --- | --- |
| `motor_driver` | 初始化、使能/失能、停止、方向与 PWM 占空比的接口契约。 |
| `motor_port_stm32` | 将驱动操作映射到 PA8、PB0、PB1、PB10 及 TIM1_CH1。 |
| `encoder_driver` | 将 16 位硬件计数扩展为可读的累计位置，并处理计数器跨界。 |
| `encoder_port_stm32` | 使用 TIM2 编码器模式读取 PA0/PA1 的 AB 相计数。 |

正式 F411 工程位于 [`firmware_f411/f411_actuator_ecu`](firmware_f411/f411_actuator_ecu)，模块源码位于其 [`BSP`](firmware_f411/f411_actuator_ecu/BSP) 目录。

---

## 仓库地图

| 目录 | 内容 |
| --- | --- |
| [`firmware_s32k/`](firmware_s32k) | S32K144 主 ECU：平台、通信、诊断、应用与 Bootloader。 |
| [`firmware_f411/`](firmware_f411) | STM32F411 执行器 ECU：BSP、应用、LIN 从节点与安全逻辑。 |
| [`tools_pc/`](tools_pc) | PC 端调试、诊断、刷写和自动化工具。 |
| [`config/`](config) | CAN、LIN、DID、DTC 等配置源。 |
| [`hardware/`](hardware) | BOM、接线、照片和测量记录。 |
| [`docs/`](docs) | 项目基线、进度计划、接口、测试和演示文档。 |

---

## 项目边界

这是一个按工程方法逐步实现和验证的实验平台，不将未完成内容包装为既有能力：

- UDS、DTC、NvM、CAN Bootloader、完整 CAN-LIN 网关仍在后续阶段。
- 不宣称已达到量产车规、OEM 密钥管理、Secure Boot 或 ISO 26262 认证等级。
- 每完成一个阶段，都会以编译、硬件行为、抓帧或日志作为可复查证据，并记录在 [进度计划](docs/v2.2_progress_plan.md)。

---

## 阅读顺序

1. [V2.2 项目基线](docs/project_baseline_v2.2.md)：项目范围、架构与接口基线。
2. [V2.2 实施进度](docs/v2.2_progress_plan.md)：当前任务、验收条件和已确认证据。
3. [`firmware_f411/f411_actuator_ecu`](firmware_f411/f411_actuator_ecu)：正在迭代的执行器 ECU 正式工程。

_最后更新：2026-08-04_
