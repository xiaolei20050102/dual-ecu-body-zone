# W2-L1-04：F411 本地安全状态机设计基线

> 状态：设计已确认，**尚未实现**。本文件是下一次编码的唯一设计输入；不把本文中的计划表述为已完成能力。

## 1. 阶段目标与边界

本阶段让 STM32F411 在不依赖 LIN、CAN、UDS 或 DTC 持久化的条件下，独立完成电机本地安全闭环：到达对应限位时停车；传感器或执行异常时先本地安全停机并锁存故障。

本阶段已有证据：

- `Motor_*` 已通过 TB6612FNG + N20 的正转、停止、反转、失能硬件验证。
- 编码器位置快照已通过正反向计数验证。
- 双限位输入（PB12/PB13，主动低）已完成去抖，并已验证 `NONE`、`OPEN_ACTIVE`、`CLOSE_ACTIVE`、`CONFLICT` 四种稳定状态及 USART1 日志。

本阶段不做：LIN 协议解析、CAN-LIN 网关、UDS、DTC/NvM、完整闭环位置控制。LIN 只作为未来命令来源，不能直接控制电机。

## 2. 分层和数据流

```text
PB12/PB13 -> limit_port -> limit_driver -> 限位稳定快照 --+
TIM2      -> encoder_port -> encoder_driver -> 编码器快照 --+-> ControlTask
临时命令（以后替换为 LIN 已校验命令） -----------------------+        |
当前时间 / 超时监视 ------------------------------------------+        v
                                                        状态机 Update
                                                               |
                                  状态快照 <-------------------+----------------> Motor_*
                                      |                                          |
                                  USART1 日志                              TB6612FNG -> N20
```

规则：

1. 状态机不直接读 GPIO、不直接读 TIM2、不解析 LIN 帧、不写 PWM/GPIO 寄存器。
2. `ControlTask` 每 10 ms 读取各驱动快照，组成本周期输入，再调用状态机。
3. 状态机只通过 `Motor_*` 动作；禁止越过 `motor_driver` 操作 TIM1、AIN1/AIN2 或 STBY。
4. LIN 接入后只替换“命令输入来源”；状态机规则不变。

## 3. 名词边界

### 3.1 纯状态机

状态机的核心是：`当前状态 + 输入事件 -> 新状态 + 安全动作`。

### 3.2 状态机模块

状态机模块除核心迁移外，还保存运行上下文，例如当前故障码、动作开始时间、上次编码器变化时间。命令、限位和编码器属于输入；状态和故障码同时属于模块内部状态，并通过输出快照供日志和未来 LIN 读取。

## 4. 状态、命令与故障码

### 4.1 状态

| 状态 | 含义 | 电机状态 |
|---|---|---|
| `IDLE` | 健康、停止失能、位置未知（通常在中间或上电未触发限位） | PWM=0，STBY=0 |
| `MOVING_OPEN` | 健康、向开端运动 | 已使能 |
| `MOVING_CLOSE` | 健康、向关端运动 | 已使能 |
| `AT_OPEN` | 健康、停止失能、确认在开端 | PWM=0，STBY=0 |
| `AT_CLOSE` | 健康、停止失能、确认在关端 | PWM=0，STBY=0 |
| `FAULT` | 故障锁存；拒绝运动命令 | PWM=0，STBY=0 |

`IDLE` 不等于“必然在中间”；它只代表健康但没有已确认的端点位置。`AT_OPEN` 与 `AT_CLOSE` 不能合并进 `IDLE`，因为它们保留了端点安全信息。

### 4.2 命令

| 命令 | 含义 |
|---|---|
| `OPEN` | 请求向开端运动 |
| `CLOSE` | 请求向关端运动 |
| `STOP` | 请求安全停止 |
| `CLEAR_FAULT` | 仅请求解除故障锁存；不等于重新启动 |
| `NONE` | 本周期无新命令 |

### 4.3 故障码

```text
LIMIT_CONFLICT
LIMIT_INPUT_INVALID
LIMIT_OPEN_STUCK
LIMIT_CLOSE_STUCK
LIMIT_OPEN_POSITION_LOST
LIMIT_CLOSE_POSITION_LOST
ENCODER_UPDATE_ERROR
ENCODER_STALL
COMMAND_TIMEOUT
TRAVEL_TIMEOUT_OPEN
TRAVEL_TIMEOUT_CLOSE
MOTOR_DRIVER_ERROR
```

## 5. 统一电机动作

```text
SAFE_STOP
  = Motor_Stop() -> Motor_Disable()
  = 最终必须满足 PWM=0、STBY=0

SAFE_START(方向)
  = 确认已安全停止
  -> Motor_SetDirection(方向)
  -> Motor_SetDuty(配置的占空比)
  -> Motor_Enable()
```

`MOTOR_DIR_FORWARD` 与“开方向/关方向”的实际对应关系只在一处配置。当前可先采用占位映射；硬件验收时以实际机构方向为准调整。应用层不直接改方向引脚或 PWM。

## 6. 事件优先级

每次 `Update()` 先后按以下优先级判断：

```text
1. 输入可信度：限位读取、编码器更新、电机动作结果
2. 立即危险：双限位冲突
3. 运动异常：通信超时、行程超时、编码器堵转、反向端限位未释放
4. 正常到位：当前运动方向的目标限位有效
5. 当前是否处于 FAULT：仅允许处理 CLEAR_FAULT
6. 普通命令：OPEN / CLOSE / STOP
```

因此在同一周期同时出现开到位和 `OPEN` 命令时，必须先处理“开到位”，立即停止，不能因命令多运行一个 10 ms 周期。

## 7. 上电初始状态

初始限位状态经去抖确认后：

| 初始限位状态 | 初始状态 | 动作 |
|---|---|---|
| `CONFLICT` | `FAULT` | `SAFE_STOP`，锁存 `LIMIT_CONFLICT` |
| `OPEN_ACTIVE` | `AT_OPEN` | `SAFE_STOP` |
| `CLOSE_ACTIVE` | `AT_CLOSE` | `SAFE_STOP` |
| `NONE` | `IDLE` | `SAFE_STOP` |

## 8. 正常状态迁移表

| 当前状态 | 事件 | 新状态 | 动作 |
|---|---|---|---|
| `IDLE` | `OPEN` | `MOVING_OPEN` | `SAFE_START(开方向)`，记录开行程起点 |
| `IDLE` | `CLOSE` | `MOVING_CLOSE` | `SAFE_START(关方向)`，记录关行程起点 |
| `IDLE` | `STOP` | `IDLE` | `SAFE_STOP` |
| `MOVING_OPEN` | 开限位有效 | `AT_OPEN` | `SAFE_STOP` |
| `MOVING_CLOSE` | 关限位有效 | `AT_CLOSE` | `SAFE_STOP` |
| `MOVING_OPEN` | `OPEN` | `MOVING_OPEN` | 保持当前输出，不重复启动 |
| `MOVING_CLOSE` | `CLOSE` | `MOVING_CLOSE` | 保持当前输出，不重复启动 |
| `MOVING_OPEN` | `STOP` | `IDLE` | `SAFE_STOP` |
| `MOVING_CLOSE` | `STOP` | `IDLE` | `SAFE_STOP` |
| `MOVING_OPEN` | `CLOSE` | `IDLE` | `SAFE_STOP`；丢弃本条反向请求，等待下一条 `CLOSE` |
| `MOVING_CLOSE` | `OPEN` | `IDLE` | `SAFE_STOP`；丢弃本条反向请求，等待下一条 `OPEN` |
| `MOVING_OPEN` | 单独关限位有效 | `MOVING_OPEN` | 允许继续离开关端；后续检查是否及时释放 |
| `MOVING_CLOSE` | 单独开限位有效 | `MOVING_CLOSE` | 允许继续离开开端；后续检查是否及时释放 |
| `AT_OPEN` | `OPEN` / `STOP` | `AT_OPEN` | 保持 `SAFE_STOP`，拒绝继续开 |
| `AT_OPEN` | `CLOSE` | `MOVING_CLOSE` | `SAFE_START(关方向)` |
| `AT_CLOSE` | `CLOSE` / `STOP` | `AT_CLOSE` | 保持 `SAFE_STOP`，拒绝继续关 |
| `AT_CLOSE` | `OPEN` | `MOVING_OPEN` | `SAFE_START(开方向)` |

直接反向被禁止是为了避免电机尚未停止时换向。`AT_OPEN -> MOVING_CLOSE` 和 `AT_CLOSE -> MOVING_OPEN` 可以直接开始，因为端点状态已保证电机已经停止并失能。

## 9. 全局故障迁移表

以下规则优先于普通迁移；第一次进入 `FAULT` 时执行一次 `SAFE_STOP`、锁存故障码并输出一次日志。后续周期保持 `FAULT`，不重复刷屏或自动恢复。

| 适用状态 | 事件 | 新状态 | 故障码 |
|---|---|---|---|
| 任意健康状态 | 双限位同时有效 | `FAULT` | `LIMIT_CONFLICT` |
| 任意健康状态 | 限位驱动读取失败 | `FAULT` | `LIMIT_INPUT_INVALID` |
| 任意健康状态 | 编码器更新失败或累计溢出 | `FAULT` | `ENCODER_UPDATE_ERROR` |
| 任意健康状态 | 关键 `Motor_*` 返回失败 | `FAULT` | `MOTOR_DRIVER_ERROR` |
| `MOVING_OPEN` / `MOVING_CLOSE` | 通信命令超时 | `FAULT` | `COMMAND_TIMEOUT` |
| `MOVING_OPEN` | 最大开行程时间内未到开端 | `FAULT` | `TRAVEL_TIMEOUT_OPEN` |
| `MOVING_CLOSE` | 最大关行程时间内未到关端 | `FAULT` | `TRAVEL_TIMEOUT_CLOSE` |
| `MOVING_OPEN` | 起步宽限结束后关限位仍有效 | `FAULT` | `LIMIT_CLOSE_STUCK` |
| `MOVING_CLOSE` | 起步宽限结束后开限位仍有效 | `FAULT` | `LIMIT_OPEN_STUCK` |
| `MOVING_OPEN` / `MOVING_CLOSE` | 编码器长期无足够计数变化 | `FAULT` | `ENCODER_STALL` |
| `AT_OPEN` | 未收到 `CLOSE` 时开限位稳定释放 | `FAULT` | `LIMIT_OPEN_POSITION_LOST` |
| `AT_CLOSE` | 未收到 `OPEN` 时关限位稳定释放 | `FAULT` | `LIMIT_CLOSE_POSITION_LOST` |

## 10. 故障复位

| 当前状态 | 事件 | 条件 | 新状态 | 动作 |
|---|---|---|---|---|
| `FAULT` | `OPEN` / `CLOSE` / `STOP` | 任意 | `FAULT` | 保持 `SAFE_STOP`，拒绝运动 |
| `FAULT` | `CLEAR_FAULT` | 对应故障条件仍存在 | `FAULT` | 拒绝复位 |
| `FAULT` | `CLEAR_FAULT` | 对应故障条件已消失 | `IDLE` | 清故障码，仍保持 `SAFE_STOP` |

故障复位不等于启动。进入 `IDLE` 后必须再收到新的 `OPEN` 或 `CLOSE` 才允许运动。

## 11. 通信新鲜度与未来 LIN

动作启动是一次命令触发；运动期间需要周期有效通信作为“继续运动授权”。

```text
有效 LIN BodyCmd
  = PID/长度/命令范围/CRC/Alive 均正确
  -> LIN 接收模块刷新 last_valid_command_time
  -> 输出 OPEN / CLOSE / STOP / CLEAR_FAULT 给状态机

若状态为 MOVING_OPEN 或 MOVING_CLOSE：
  now - last_valid_command_time > T_COMMAND_TIMEOUT
  -> COMMAND_TIMEOUT -> FAULT -> SAFE_STOP
```

所有有效 LIN 帧都可刷新通信新鲜度；表中“同方向命令保持运动”不是唯一的刷新入口。`IDLE`、端点状态和 `FAULT` 已安全停止，不因没有通信而进入故障。

当前未接入 LIN：临时命令由 `ControlTask` 提供；`COMMAND_TIMEOUT` 事件先保留在接口和状态表中，不在单板首次限位停车验证中启用。

## 12. 待标定参数

以下是配置项，不得凭空写固定数值；要在实际机构上测量后记录来源和最终值。

```text
T_LIMIT_DEBOUNCE          限位去抖时间（当前驱动为 3 个 10 ms 一致采样）
T_LIMIT_RELEASE           离开反向端后限位必须释放的最大时间
T_ENCODER_STARTUP_GRACE   启动后允许编码器暂时不变化的宽限时间
T_ENCODER_STALL           宽限结束后允许无计数变化的最大时间
ENCODER_MIN_DELTA         判定“仍在运动”的最小计数变化
T_TRAVEL_OPEN             最大开行程时间
T_TRAVEL_CLOSE            最大关行程时间
T_COMMAND_TIMEOUT         LIN 有效命令新鲜度超时
```

## 13. 状态机模块的概念接口

接口保持少而完整：

```text
Init
  上电执行一次；根据初始限位进入安全初始状态。

Update(Input)
  ControlTask 每 10 ms 调用；输入为限位、编码器、命令、时间和通信健康信息。
  状态机按第 6 节优先级更新内部状态，并通过 Motor_* 执行必要动作。

GetSnapshot
  只读输出当前状态与故障码，供 USART1 和未来 LIN 状态报文使用。
```

概念输入包：

```text
limit_valid / limit_state
encoder_valid / encoder_position
command
command_fresh（当前单板阶段可保留占位）
now_ms
```

模块内部保存：

```text
current_state
latched_fault
motion_start_time
last_encoder_position
last_encoder_change_time
```

输出快照：

```text
current_state
latched_fault
```

## 14. 明日编码顺序（用户动手）

每步完成后先编译并发代码/截图给 Codex 审查；不要一次写完整状态机。

1. 新建 `BSP/Inc/actuator_state_machine_types.h`：只写 include guard、状态枚举、命令枚举、故障枚举。
2. 新建 `BSP/Inc/actuator_state_machine.h`：只定义状态机对象包含的运行上下文，并声明 `Init`、`Update`、`GetSnapshot`。
3. 新建 `BSP/Src/actuator_state_machine.c`：先实现 `Init`，验证上电只安全停机并正确判定四种限位初态。
4. 实现 `Update` 的第一条正常闭环：`IDLE + OPEN -> MOVING_OPEN`，`MOVING_OPEN + 开限位 -> AT_OPEN`。
5. 以相同方式实现关方向闭环。
6. 加入双限位冲突的故障锁存；验证 `PWM=0`、`STBY=0` 与一次故障日志。
7. 再逐项加入 STOP、禁止直接反转、`CLEAR_FAULT`、编码器更新失败、各类超时与堵转判断。
8. 每一步只修改必要文件和 Keil 工程条目；保持 `0 Error(s), 0 Warning(s)`。

## 15. 本阶段硬件验收出口

必须保存构建结果、USART1 日志和硬件行为证据：

```text
正常：正转触发开到位 -> AT_OPEN -> PWM=0、STBY=0
正常：反转触发关到位 -> AT_CLOSE -> PWM=0、STBY=0
异常：双限位冲突 -> FAULT_LIMIT_CONFLICT -> PWM=0、STBY=0
异常：编码器更新失败（可在软件中受控注入） -> FAULT
异常：超时事件（后续受控注入） -> FAULT
恢复：故障消失 + CLEAR_FAULT -> IDLE，电机不自动重启
```

通过本阶段后才进入 5 ms 任务周期、抖动、栈水位等 FreeRTOS 运行证据；不得提前开始 LIN。
