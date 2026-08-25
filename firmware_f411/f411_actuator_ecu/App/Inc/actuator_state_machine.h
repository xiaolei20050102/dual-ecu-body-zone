#ifndef ACTUATOR_STATE_MACHINE_H
#define ACTUATOR_STATE_MACHINE_H

#include "actuator_sm_types.h"
#include "motor_driver.h"

/**
 * @brief 执行器状态机的运行时对象。
 *
 * @note 该对象保存状态机的内部状态，并绑定一个已初始化的 MotorHandle_t
 *       和只读执行器配置。当前仅允许 ControlTask 在单一任务上下文中调用
 *       Actuator_Init()、Actuator_Update() 和 Actuator_GetSnapshot()；
 *       不允许多个任务同时访问同一对象。
 */
typedef struct
{
    MotorHandle_t *motor;                 // 绑定的底层电机驱动；状态机只能通过该对象调用 Motor_* 控制电机。
    const ActuatorConfig_t *config;       // 绑定的只读机构标定；运行期间只读取，不允许状态机修改。
    bool initialized;                     // 仅当初始化已完成安全停机和失能后才为 true；false 时 Update() 必须拒绝处理。

    ActuatorState_t current_state;        // 当前执行器状态，例如 INIT、IDLE、MOVING_OPEN 或 FAULT。
    ActuatorFault_t latched_fault;        // 首次进入 FAULT 时记录的故障原因；故障未被允许清除前保持不变。

    uint32_t motion_start_time_ms;        // 本次开始向开端或关端运动的时间，用于限位释放和最大行程超时判断。
    EncoderPosition_t last_encoder_position; // 上一次确认电机确实在动时的编码器位置，用于判断本周期是否有足够位移。
    uint32_t last_encoder_change_time_ms; // 上一次确认电机确实在动的时间；超过堵转超时仍无足够位移时进入 FAULT。
} ActuatorHandle_t;

/**
 * @brief 绑定执行器状态机依赖，并进入安全的 INIT 状态。
 * @param[out] p_actuator_handle 待初始化的执行器状态机对象。
 * @param[in,out] p_motor 已完成 Motor_Init() 的电机驱动对象。
 * @param[in] p_config 执行器只读标定配置。
 * @return 接口调用结果。
 * @retval ACTUATOR_STATUS_OK 初始化成功，电机已被请求进入安全停止状态。
 * @pre p_actuator_handle、p_motor 和 p_config 均为有效指针，且 p_motor 已初始化。
 * @note 初始化完成后仍处于 INIT；必须等到限位输入首次稳定后，才允许处理运动命令。
 */
ActuatorStatus_t Actuator_Init(ActuatorHandle_t *p_actuator_handle,MotorHandle_t *p_motor,const ActuatorConfig_t *p_config);

/**
 * @brief 使用本控制周期的输入快照推进执行器状态机。
 * @param[in,out] p_actuator_handle 已初始化的执行器状态机对象。
 * @param[in] p_input 本周期由 ControlTask 组装的只读输入快照。
 * @return 接口调用结果。
 * @retval ACTUATOR_STATUS_OK 本周期已完成状态机处理。
 * @pre 必须在同一 ControlTask 中周期调用；p_input 的时间基准应单调递增。
 * @note 本接口只通过 Motor_* 控制电机；发生安全异常时进入 FAULT 并锁存首个故障。
 */
ActuatorStatus_t Actuator_Update(ActuatorHandle_t *p_actuator_handle,const ActuatorInput_t *p_input);

/**
 * @brief 读取执行器当前状态与已锁存故障的快照。
 * @param[in] p_actuator_handle 已初始化的执行器状态机对象。
 * @param[out] p_actuator_snapshot 用于接收当前状态快照的输出对象。
 * @return 接口调用结果。
 * @retval ACTUATOR_STATUS_OK 快照读取成功。
 * @pre 仅可读取已完成 Actuator_Init() 的对象。
 * @note 本接口不改变状态机状态，可供日志和后续通信模块读取。
 */
ActuatorStatus_t Actuator_GetSnapshot(const ActuatorHandle_t *p_actuator_handle,ActuatorSnapshot_t *p_actuator_snapshot);

#endif /* ACTUATOR_STATE_MACHINE_H */
