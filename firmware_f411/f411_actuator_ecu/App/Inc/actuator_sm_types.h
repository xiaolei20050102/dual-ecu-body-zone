#ifndef ACTUATOR_SM_TYPES_H
#define ACTUATOR_SM_TYPES_H
#include <stdbool.h>
#include <stdint.h>
#include "limit_types.h"
#include "encoder_types.h"
#include "motor_types.h"

/**
 * @brief 执行器状态机的当前运行状态。
 * @note 状态决定电机是否允许运动；FAULT 状态下拒绝所有运动命令。
 */
typedef enum 
{
    ACTUATOR_STATE_IDLE = 0,//健康、停止失能、位置未知（通常在中间或上电未触发限位）
    ACTUATOR_STATE_MOVING_OPEN,//健康、向开端运动
    ACTUATOR_STATE_MOVING_CLOSE,//健康、向关端运动
    ACTUATOR_STATE_AT_OPEN,//健康、停止失能、确认在开端
    ACTUATOR_STATE_AT_CLOSE,//健康、停止失能、确认在关端
    ACTUATOR_STATE_FAULT,//故障锁存；拒绝运动命令

} ActuatorState_t;

/**
 * @brief 输入给执行器状态机的动作命令。
 * @note 当前由 ControlTask 提供；后续仅允许由已校验的 LIN 命令转换得到。
 */
typedef enum 
{
    ACTUATOR_CMD_NONE = 0,//本周期无新命令
    ACTUATOR_CMD_OPEN,//请求向开端运动
    ACTUATOR_CMD_CLOSE,//请求向关端运动
    ACTUATOR_CMD_STOP,//请求安全停止
    ACTUATOR_CMD_CLEAR_FAULT,//仅请求解除故障锁存,不等于重新启动

} ActuatorCommand_t;

/**
 * @brief 执行器状态机锁存的本地故障原因。
 * @note 首次进入 FAULT 时记录；CLEAR_FAULT 仅在故障条件消失后允许清除。
 */
typedef enum 
{   
    ACTUATOR_FAULT_NONE = 0,//没有故障
    ACTUATOR_FAULT_LIMIT_CONFLICT,//限位器冲突，比如开和关同时生效
    ACTUATOR_FAULT_LIMIT_INPUT_INVALID,//限位驱动本身读数失败或输入不可信
    ACTUATOR_FAULT_LIMIT_OPEN_STUCK,//向开方向运动后，原先的关限位在规定时间内仍未释放；可能开关卡住、线路短路，或机构没动
    ACTUATOR_FAULT_LIMIT_CLOSE_STUCK,//向关方向运动后，原先的开限位仍未及时释放；和上面镜像
    ACTUATOR_FAULT_LIMIT_OPEN_POSITION_LOST,//已经确认在开端，但没有收到“关”命令，开限位却稳定释放了；说明位置依据丢失，可能有人手动移动机构或传感器异常
    ACTUATOR_FAULT_LIMIT_CLOSE_POSITION_LOST,//已经确认在关端，但关限位却意外释放；镜像情况同上
    ACTUATOR_FAULT_ENCODER_UPDATE_ERROR,//编码器驱动更新失败，例如底层读取失败、计数处理检测到不可接受的异常
    ACTUATOR_FAULT_ENCODER_STALL,//电机被命令运动，但经过启动宽限后编码器长期没有足够的计数变化；可能堵转、脱线或电机没转
    ACTUATOR_FAULT_COMMAND_TIMEOUT,//运动中长时间收不到有效命令心跳；F411 不再确认上位控制是否还在线，因此本地停车
    ACTUATOR_FAULT_TRAVEL_TIMEOUT_OPEN,//持续向开端运动，超过最大允许行程时间仍未触发开限位
    ACTUATOR_FAULT_TRAVEL_TIMEOUT_CLOSE,//原理同上
    ACTUATOR_FAULT_MOTOR_DRIVER_ERROR,//调用 Motor_SetDirection、Motor_SetDuty、Motor_Enable、Motor_Stop 或 Motor_Disable 接口返回失败

} ActuatorFault_t;

/**
 * @brief 执行器状态机单个控制周期的输入快照。
 * @note 由 ControlTask 从限位、编码器、命令和时间基准组装；状态机不直接读取硬件。
 */
typedef struct 
{
    bool limit_valid;
    LimitSwitchState_t limit_state;
    bool encoder_valid;
    EncoderPosition_t encoder_position;
    ActuatorCommand_t command;
    bool command_fresh;
    uint32_t now_ms;

}ActuatorInput_t;

/**
 * @brief 执行器状态机对外发布的只读状态快照。
 * @note 供 USART1 日志和后续 LIN 状态报文使用。
 */
typedef struct 
{
    ActuatorState_t current_state;
    ActuatorFault_t latched_fault;
}ActuatorSnapshot_t;


/**
 * @brief 执行器机构相关的只读标定配置。
 * @note 方向、占空比和超时阈值由机构测量后填写；状态机运行期间不得修改。
 */
typedef struct 
{
    MotorDirection_t open_direction;
    MotorDirection_t close_direction;
    uint8_t  run_duty_percent;
    uint32_t limit_release_timeout_ms;
    uint32_t encoder_startup_grace_ms;
    uint32_t encoder_stall_timeout_ms;
    uint32_t encoder_min_delta;
    uint32_t travel_open_timeout_ms;
    uint32_t travel_close_timeout_ms;
    uint32_t command_timeout_ms;
}ActuatorConfig_t;
#endif /*ACTUATOR_SM_TYPES_H*/
