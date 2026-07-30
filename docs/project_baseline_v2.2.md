# 双 ECU 车身区域控制诊断刷写实验平台：项目基线 V2.2

生效日期：2026-07-26

本文件定义仓库实施、验收和简历表述的唯一边界。完整说明书为《双ECU车身区域控制诊断刷写实验平台_项目说明书_v2.2》；代码、协议、测试和演示均以本基线为准。

## 1. 固定架构

```text
PC 诊断/测试工具
    └─ Classic CAN 500 kbit/s
       S32K144 主 ECU
          └─ LIN 19.2 kbit/s
             STM32F411 执行器 ECU
                └─ TB6612 + N20 编码器电机 + 双限位
```

- S32K144：CAN、LIN Master、信号级网关、模式管理、ISO-TP/UDS、DTC/NvM、看门狗、CAN Bootloader。
- STM32F411：LIN Slave、PWM、编码器、限位、电机状态机、本地安全停机。
- PC 工具：控制、监控、UDS 客户端、刷写、故障注入、配置校验、自动回归和测试报告。

## 2. 技术含金量的取舍原则

项目不以堆砌 AUTOSAR、CAN FD、以太网或密码学名词取胜，而以“能运行、能测量、能解释、能回归”的 ECU 工程链路取胜。

| 优先级 | 招聘对应能力 | 本项目的交付 |
|---|---|---|
| 核心必做 | C、MCU 驱动、CAN/LIN、PWM/编码器/限位、J-Link 调试、Git | 双板独立运行，CAN 主干与 LIN 执行器链路可演示 |
| 核心必做 | RTOS、状态机、看门狗、故障降级、软硬件联调 | 双 ECU 模式管理、通信超时、本地安全停机、时序测量 |
| 核心必做 | UDS、DTC、非易失存储、诊断测试 | ISO-TP/UDS 服务矩阵、DTC 生命周期、NvM A/B 和掉电恢复 |
| 核心必做 | Bootloader、Flash、Python/PC 工具、自动化测试 | CAN 刷写、Manifest、边界/CRC/中断恢复、回归报告 |
| 进阶增强 | 镜像真实性、密钥边界、防回滚 | 数字签名与防回滚策略；完成后以实测证据作为安全增强模块展示 |

“进阶增强”不是删除项，而是不应在未完成前写成所有岗位的基础能力或已经量产的安全方案。

## 3. 工程设计架构（开发方向）

### 3.1 分层与职责

| 层级 | S32K144 主 ECU | STM32F411 执行器 ECU | PC 工具 |
|---|---|---|---|
| 应用层 | ZoneControl、ModeManager、GatewayPolicy、FaultReaction | ActuatorStateMachine、LocalSafety | Control、Monitor、故障注入 |
| 服务层 | UdsServer、DtcManager、NvmManager、WdgManager、Version | 参数/校准、Watchdog、故障快照 | ISO-TP/UDS 客户端、刷写、报告 |
| 通信层 | CanIf/Com/CanTp、LinScheduler、信号数据库 | LinSlave、命令/状态 PDU | python-can、DBC 解码、测试适配器 |
| 平台/BSP | FreeRTOS、时基、队列、FlexCAN、LPUART-LIN、Flash、WDG | 定时器编码器、PWM、GPIO、USART-LIN、WDG | USB-CAN 驱动与 Python 运行环境 |

依赖方向只能从上到下：应用层不直接修改寄存器；中断只做收包、时间戳和事件投递；协议解析、Flash 写入、日志和策略判断在任务上下文完成。

### 3.2 四条必须跑通的链路

1. **控制链路**：PC 的 `VehicleCommand` → S32 校验/模式仲裁 → LIN `BodyCmd` → F411 状态机/PWM → 电机与限位。
2. **状态链路**：编码器/限位/本地故障 → F411 `ActuatorStatus` → S32 快照/DTC → CAN `VehicleStatus` → PC 监视。
3. **故障链路**：F411 命令超时、限位矛盾、编码器失效先本地 `PWM=0`；S32 将远端状态映射为 DTC、进入 DEGRADED/SAFE 并禁止新动作。
4. **诊断刷写链路**：PC UDS 客户端 → S32 ISO-TP/UDS → DID/DTC/NvM 或编程会话 → Bootloader 验证/写入/复位 → 版本 DID 复核。

### 3.3 设计源与接口冻结

- `config/` 中的 DBC、LIN 信号矩阵、DID/DTC 矩阵是唯一设计源；PC、S32、F411 不分别维护不一致的报文 ID、比例和超时常量。
- 每条跨 ECU 信号必须写清 ID/PID、长度、单位、缩放、范围、无效值、周期、超时、Alive/CRC 和接收后的安全动作。
- 每个模块只有一份公开头文件/接口契约；用需求 ID、测试 ID 和证据路径连接到实现与回归报告。

## 4. 核心最终功能（必须完成并有证据）

1. 双 ECU 模式：INIT、NORMAL、DEGRADED、DIAGNOSTIC、PROGRAMMING、SAFE。
2. CAN-LIN 信号级网关：ID/长度/范围、Alive、CRC、新鲜度、缩放、无效值、周期转换和恢复去抖；禁止原样转发报文。
3. 双侧故障隔离：检测方先执行本地安全动作再上报；通信恢复不得自动恢复危险动作。
4. FreeRTOS：任务优先级、队列/事件、看门狗监督、WCET、抖动、栈水位与 CPU 资源证据。
5. UDS：0x10、0x11、0x22、0x2E、0x3E、0x19、0x14、0x27、0x31、0x34/0x36/0x37；实现会话、权限、NRC、P2/P2*、S3。
6. DTC/NvM：故障去抖、状态、快照、清除条件；A/B 副本、CRC、版本、序号、有效标志与掉电恢复。
7. CAN Bootloader：受保护分区、块序号、Manifest、地址/长度校验、整镜像 CRC、无效 App 保护、下载中断恢复与版本可追溯。
8. 工程验证：DBC/LIN/DID/DTC 配置一致性、需求追踪、单元/集成/系统测试、故障注入、自动回归与发布门禁。

## 5. 安全增强模块（完成后纳入最终演示）

1. Manifest 增加目标硬件、软件版本、构建 ID、镜像长度、入口地址、CRC 与签名字段。
2. Bootloader 只保存验证公钥或公钥摘要；私钥不进入仓库、不写入 ECU。
3. 对 Manifest 关键字段和镜像做数字签名验证；CRC 仍仅负责完整性，不能替代来源验证。
4. 使用单调版本或最低允许版本实现防回滚；记录授权、拒绝原因和验证结果。
5. 明确实验边界：不宣称 OEM 密钥管理、HSM、量产 Secure Boot 或 ASIL 认证。

## 6. 实施与简历约束

- 按 L1 基础链路 → L2 分布式控制 → L3 诊断可靠性 → L4 刷写工程化 → L4-S 安全增强的顺序集成。
- L1-L4 是项目核心交付；L4-S 是完成后展示的高阶亮点，不因文档存在就声称已实现。
- 每个“已完成”功能必须同时具备：源码/配置提交、可解释接口与状态机、正常与错误路径测试、日志/抓帧/硬件动作证据、需求 ID 与测试用例、README 运行方法和已知限制。
- 不表述为完整 AUTOSAR、ASIL、OEM 密钥管理或量产 HIL；可表述为“按 AUTOSAR Classic 职责思想进行分层设计”。
- 只有实际完成并保存证据后，才在简历中写“实现数字签名/防回滚”或“实现安全刷写”。
