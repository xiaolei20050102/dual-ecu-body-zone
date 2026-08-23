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
PB12/PB13 -> limit_port -> limit_driver -> 限位稳定快照（state + stable_valid） --+
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
2. `ControlTask` 每 10 ms 读取各驱动快照，组成本周期输入，再调用状态机；当前阶段由它独占 `Motor_*` 调用权。
3. 状态机只通过 `Motor_*` 动作；禁止越过 `motor_driver` 操作 TIM1、AIN1/AIN2 或 STBY。
4. LIN 接入后只替换“命令输入来源”；状态机不解析 PID、长度、CRC 或 Alive。
5. `SafetyTask` 不得并发调用 `Motor_*` 或直接修改 PWM、方向和 STBY；它后续只负责健康监视、看门狗或复位策略。
6. 状态机只发布状态和故障快照；USART1 日志由任务层仅在快照变化时输出。进入 5 ms 周期后，日志不得在控制路径中长期阻塞。

## 3. 名词边界

### 3.1 纯状态机

状态机的核心是：`当前状态 + 输入事件 -> 新状态 + 安全动作`。

### 3.2 状态机模块

状态机模块除核心迁移外，还保存运行上下文，例如当前故障码、动作开始时间、上次编码器变化时间。命令、限位和编码器属于输入；状态和故障码同时属于模块内部状态，并通过输出快照供日志和未来 LIN 读取。

## 4. 状态、命令与故障码

### 4.1 状态

| 状态 | 含义 | 电机状态 |
|---|---|---|
| `INIT` | 上电等待限位输入完成首次去抖；拒绝全部运动命令 | PWM=0，STBY=0 |
| `IDLE` | 健康、停止失能、位置未知（通常在中间或上电未触发限位） | PWM=0，STBY=0 |
| `MOVING_OPEN` | 健康、向开端运动 | 已使能 |
| `MOVING_CLOSE` | 健康、向关端运动 | 已使能 |
| `AT_OPEN` | 健康、停止失能、确认在开端 | PWM=0，STBY=0 |
| `AT_CLOSE` | 健康、停止失能、确认在关端 | PWM=0，STBY=0 |
| `FAULT` | 故障锁存；拒绝运动命令 | PWM=0，STBY=0 |

`INIT` 不是健康可运动状态：`limit_read_valid=true` 但 `limit_stable=false` 时必须保持 `INIT` 与 `SAFE_STOP`。`IDLE` 不等于“必然在中间”；它只代表健康但没有已确认的端点位置。`AT_OPEN` 与 `AT_CLOSE` 不能合并进 `IDLE`，因为它们保留了端点安全信息。

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

### 5.1 SAFE_STOP：停止失败时仍必须继续兜底

```text
SAFE_STOP
  1. 调用 Motor_Stop()，保存 stop_result
  2. 无论 stop_result 成功或失败，都调用 Motor_Disable()，保存 disable_result
  3. 若两步均成功：返回 SAFE_STOP_OK
  4. 若任一步失败：调用 Motor_ForceSafeState()，返回 SAFE_STOP_FAILED
```

`Motor_ForceSafeState()` 是最后一道硬件动作兜底：直接请求 `PWM=0`、`AIN1=0`、`AIN2=0`、`STBY=0`。当前底层接口不提供硬件反馈，因此状态机只能确认“强制安全输出已被请求”，不能把它表述为硬件状态已被反馈确认。

任何原本计划进入 `IDLE`、`AT_OPEN` 或 `AT_CLOSE` 的迁移，只有在 `SAFE_STOP_OK` 时才可进入该健康状态；若得到 `SAFE_STOP_FAILED`，必须改为进入 `FAULT` 并锁存 `MOTOR_DRIVER_ERROR`。故障入口只执行一次该兜底序列，后续 `FAULT` 周期保持停止失能且不重复刷屏。

### 5.2 SAFE_START：任一步失败都回到安全故障出口

```text
SAFE_START(方向)
  = 确认已安全停止
  -> Motor_SetDirection(方向)
  -> Motor_SetDuty(配置的占空比)
  -> Motor_Enable()

若上述任一步失败：
  -> SAFE_STOP
  -> FAULT + MOTOR_DRIVER_ERROR
```

因此状态机不允许在任何 `Motor_*` 调用失败后继续保持 `MOVING_OPEN`、`MOVING_CLOSE` 或切换到正常停止状态。

`MOTOR_DIR_FORWARD` 与“开方向/关方向”的实际对应关系只在一处配置。当前可先采用占位映射；硬件验收时以实际机构方向为准调整。应用层不直接改方向引脚或 PWM。

## 6. 事件优先级

每次 `Update()` 先后按以下优先级判断：

```text
1. 输入可信度：限位读取、限位首次稳定性、编码器更新、电机动作结果
2. 立即危险：双限位冲突
3. 运动异常：通信超时、行程超时、编码器堵转、反向端限位未释放
4. 正常到位：当前运动方向的目标限位有效
5. 当前是否处于 FAULT：仅允许处理 CLEAR_FAULT
6. 普通命令：OPEN / CLOSE / STOP
```

因此在同一周期同时出现开到位和 `OPEN` 命令时，必须先处理“开到位”，立即停止，不能因命令多运行一个 10 ms 周期。

## 7. 上电初始化与初始状态

`Actuator_Init()` 只进入 `INIT` 并执行 `SAFE_STOP`，不得根据第一次 GPIO 读取直接判定端点。`ControlTask` 持续调用限位驱动；当 `limit_read_valid=true` 且 `limit_stable=true` 后，状态机按下表离开 `INIT`：

| 首次可信限位状态 | 离开 `INIT` 后的状态 | 动作 |
|---|---|---|
| `CONFLICT` | `FAULT` | `SAFE_STOP`，锁存 `LIMIT_CONFLICT` |
| `OPEN_ACTIVE` | `AT_OPEN` | `SAFE_STOP` |
| `CLOSE_ACTIVE` | `AT_CLOSE` | `SAFE_STOP` |
| `NONE` | `IDLE` | `SAFE_STOP` |

在 `INIT` 中，`limit_read_valid=false` 立即进入 `FAULT + LIMIT_INPUT_INVALID`；`limit_stable=false` 保持 `INIT`，并忽略 `OPEN`、`CLOSE`、`STOP` 与 `CLEAR_FAULT`。

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

### 10.1 通用规则

| 当前状态 | 事件 | 条件 | 新状态 | 动作 |
|---|---|---|---|---|
| `FAULT` | `OPEN` / `CLOSE` / `STOP` | 任意 | `FAULT` | 保持 `SAFE_STOP`，拒绝运动 |
| `FAULT` | `CLEAR_FAULT` | 对应故障的复位条件未满足 | `FAULT` | 拒绝复位，保持已锁存故障码 |
| `FAULT` | `CLEAR_FAULT` | 对应故障的复位条件满足 | `IDLE` | 清故障码，仍保持 `SAFE_STOP` |

故障复位不等于启动。进入 `IDLE` 后必须再收到新的 `OPEN` 或 `CLOSE` 才允许运动。

### 10.2 故障码级复位条件

`CLEAR_FAULT` 不采用笼统的“故障消失”判断。每个故障码必须按下表判断，且任一恢复路径均不得自动使能电机。

| 已锁存故障码 | `CLEAR_FAULT` 的允许条件 | 复位含义 |
|---|---|---|
| `LIMIT_CONFLICT` | 本周期限位读取成功、首次去抖已完成，且稳定状态不是 `CONFLICT` | 已确认双限位不再同时有效。 |
| `LIMIT_INPUT_INVALID` | 本周期限位读取成功，且首次去抖已完成 | 限位输入链路已重新提供可信稳定快照。 |
| `LIMIT_OPEN_STUCK` | 本周期限位读取成功、首次去抖已完成，且开限位当前未有效 | 原开端限位已释放。 |
| `LIMIT_CLOSE_STUCK` | 本周期限位读取成功、首次去抖已完成，且关限位当前未有效 | 原关端限位已释放。 |
| `LIMIT_OPEN_POSITION_LOST` | 本周期限位读取成功、首次去抖已完成，且稳定状态不是 `CONFLICT` | 不再声称已确认开端；复位后以 `IDLE` 的位置未知语义重新开始。 |
| `LIMIT_CLOSE_POSITION_LOST` | 本周期限位读取成功、首次去抖已完成，且稳定状态不是 `CONFLICT` | 不再声称已确认关端；复位后以 `IDLE` 的位置未知语义重新开始。 |
| `ENCODER_UPDATE_ERROR` | 本周期 `Encoder_Update()` 与快照读取均成功 | 编码器链路本周期已恢复可用。 |
| `ENCODER_STALL` | 限位输入可信且编码器本周期更新成功 | 停车后无法证明堵转根因已消失；本次仅解除锁存，下一次运动必须重新执行堵转监视。 |
| `TRAVEL_TIMEOUT_OPEN` / `TRAVEL_TIMEOUT_CLOSE` | 限位输入可信且编码器本周期更新成功 | 停车后无法证明机构行程已恢复；本次仅解除锁存，下一次运动必须重新执行行程超时监视。 |
| `COMMAND_TIMEOUT` | 后续 LIN/通信模块已确认通信恢复；不得由单帧直接宣布恢复 | 通信模块应在连续有效帧满足其恢复策略后，向状态机提供通信健康输入。 |
| `MOTOR_DRIVER_ERROR` | 不允许运行中通过 `CLEAR_FAULT` 清除 | 必须保持安全停止，并重新完成 `Motor_Init()` 与 `Actuator_Init()` 后才建立新的安全基线。 |

其中 `ENCODER_STALL`、行程超时属于“仅在运动中可判断”的动态故障：故障锁存后电机已经停止，因而无法在 `FAULT` 内证明机械根因已经消失。允许人工复位的含义只是重新获得一次安全的受监视尝试机会，不是宣称故障已经被修复。

## 11. 通信新鲜度与未来 LIN

### 11.1 职责边界

动作启动是一次命令触发；运动期间需要周期有效通信作为“继续运动授权”。协议有效性与状态机安全策略必须分层：

```text
LIN 接收/通信模块
  校验 PID、长度、命令范围、CRC、Alive
  刷新 last_valid_command_time
  按 T_COMMAND_TIMEOUT 判断 communication_alive
  按连续有效帧策略判断 communication_recovered
  输出已校验命令和通信健康输入
                     |
                     v
ActuatorStateMachine
  不解析 LIN 帧
  仅在 MOVING_OPEN / MOVING_CLOSE 中：
  communication_alive=false
    -> COMMAND_TIMEOUT -> FAULT -> SAFE_STOP
```

`T_COMMAND_TIMEOUT` 属于 LIN/通信配置，不属于执行器机构标定参数。项目最终目标是 F411 在 LIN 命令超时 100 ms 内安全停止；当前未接 LIN，不启用该故障判定。通信恢复不得由单帧宣布成功，连续有效帧数量由后续 LIN 信号矩阵定义。

所有有效 LIN 帧都可刷新通信新鲜度；表中“同方向命令保持运动”不是唯一的刷新入口。`IDLE`、端点状态和 `FAULT` 已安全停止，不因没有通信而进入故障。

### 11.2 当前单板阶段的临时来源

当前临时命令由 `ControlTask` 提供。为保持状态机接口不因 LIN 接入而重写，单板阶段固定向状态机提供 `communication_alive=true`；只有 LIN 通信模块接入并经过独立验证后，才允许该输入改由通信监视器产生。

## 12. 待标定参数与周期迁移约束

以下是执行器机构配置项，不得凭空写固定数值；要在实际机构上测量后记录来源和最终值。

```text
T_LIMIT_DEBOUNCE          限位去抖时间（当前驱动为 3 个 10 ms 一致采样）
T_LIMIT_RELEASE           离开反向端后限位必须释放的最大时间
T_ENCODER_STARTUP_GRACE   启动后允许编码器暂时不变化的宽限时间
T_ENCODER_STALL           宽限结束后允许无计数变化的最大时间
ENCODER_MIN_DELTA         判定“仍在运动”的最小计数变化
T_TRAVEL_OPEN             最大开行程时间
T_TRAVEL_CLOSE            最大关行程时间
```

`T_COMMAND_TIMEOUT` 已移至通信配置，不能继续放在 `ActuatorConfig_t`。当前限位驱动使用“连续 3 次采样”去抖；在 10 ms 周期下约为 30 ms。后续控制周期迁移到 5 ms 时，不得静默沿用 3 次采样而将去抖时间改成约 15 ms；必须显式保持目标去抖时间，或记录新的标定和回归证据。

## 13. 状态机模块的概念接口

接口保持少而完整：

```text
Init
  上电执行一次；绑定电机对象和只读机构配置，验证配置合法性后进入 INIT 与 SAFE_STOP。

Update(Input)
  ControlTask 每 10 ms 调用；输入为限位、编码器、命令、时间和通信健康信息。
  状态机按第 6 节优先级更新内部状态，并通过 Motor_* 执行必要动作。

GetSnapshot
  只读输出当前状态与故障码，供 USART1 和未来 LIN 状态报文使用。
```

概念输入包：

```text
limit_read_valid / limit_stable / limit_state
encoder_valid / encoder_position
command
communication_alive（当前单板阶段固定为 true）
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

`ActuatorHandle_t` 还必须保存 `MotorHandle_t *motor`、`const ActuatorConfig_t *config` 和 `initialized`。配置校验至少包括：开、关方向必须不同；运行占空比为 1~100；所有当前已启用的机构超时和阈值非零。`ActuatorConfig_t` 不再包含 `command_timeout_ms`。

公开接口除 `Init / Update / GetSnapshot` 外，还应定义模块级返回状态 `ActuatorStatus_t`，至少能表达空指针、未初始化、无效配置和底层电机动作失败。公开头文件与实现文件固定放在 `App/Inc`、`App/Src`；Keil 工程必须增加 `../App/Inc` 包含路径并显式加入状态机 `.c` 文件。

未来若加入“目标位置/百分比”命令，不允许 LIN 直接驱动电机；通信层仍应将它转换为统一的应用命令请求，状态机再根据编码器和限位决定方向、停止与故障。该能力属于后续位置控制扩展，不在本阶段提前实现。

输出快照：

```text
current_state
latched_fault
```

## 14. 明日编码顺序（用户动手）

每步完成后先编译并发代码/截图给 Codex 审查；不要一次写完整状态机。

1. 使用已有 `App/Inc/actuator_sm_types.h`：冻结状态、命令、故障、输入/输出快照和机构配置；补齐 `INIT`、限位可信度与通信健康字段。
2. 新建 `App/Inc/actuator_state_machine.h`：定义状态机对象运行上下文、`ActuatorStatus_t`，并声明 `Init`、`Update`、`GetSnapshot`。
3. 新建 `App/Src/actuator_state_machine.c`：先实现 `Init` 与 `INIT` 分支，验证上电只安全停机；限位首次去抖完成后才正确判定四种初态。
4. 将 `App/Inc` 加入 Keil Include Path，并将 `App/Src/actuator_state_machine.c` 加入 Keil 工程；然后实现 `Update` 的第一条正常闭环：`IDLE + OPEN -> MOVING_OPEN`，`MOVING_OPEN + 开限位 -> AT_OPEN`。
5. 以相同方式实现关方向闭环。
6. 加入双限位冲突的故障锁存；验证 `PWM=0`、`STBY=0` 与一次故障日志。
7. 再逐项加入 STOP、禁止直接反转、`CLEAR_FAULT`、编码器更新失败、各类超时与堵转判断。
8. 每一步只修改必要文件和 Keil 工程条目；保持 `0 Error(s), 0 Warning(s)`。

## 15. 本阶段硬件验收出口

必须保存构建结果、USART1 日志和硬件行为证据：

```text
上电：限位尚未首次稳定 -> INIT -> PWM=0、STBY=0
正常：正转触发开到位 -> AT_OPEN -> PWM=0、STBY=0
正常：反转触发关到位 -> AT_CLOSE -> PWM=0、STBY=0
异常：双限位冲突 -> FAULT_LIMIT_CONFLICT -> PWM=0、STBY=0
异常：编码器更新失败（可在软件中受控注入） -> FAULT
异常：超时事件（后续受控注入） -> FAULT
恢复：故障消失 + CLEAR_FAULT -> IDLE，电机不自动重启
```

通过本阶段后才进入 5 ms 任务周期、抖动、栈水位等 FreeRTOS 运行证据；不得提前开始 LIN。
