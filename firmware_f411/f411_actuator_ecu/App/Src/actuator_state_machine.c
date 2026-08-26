#include "actuator_state_machine.h"
#include <stddef.h>

/**
 * @brief 请求电机进入标准安全停止状态。
 * @param[in,out] p_motor 已初始化的电机驱动对象。
 * @return 执行结果。
 * @retval ACTUATOR_STATUS_OK Motor_Stop() 与 Motor_Disable() 均成功。
 * @retval ACTUATOR_STATUS_ERR_MOTOR 标准停机任一步失败，已请求强制安全输出。
 * @pre p_motor 非空且已完成 Motor_Init()。
 * @note 本函数只处理硬件安全输出，不修改执行器状态、故障码或初始化标记。
 */
static ActuatorStatus_t Actuator_ExecuteSafeStop(MotorHandle_t *p_motor)
{
    MotorStatus_t ret_stop_status = MOTOR_STATUS_OK;
    MotorStatus_t ret_disable_status = MOTOR_STATUS_OK;
    /*先尝试手动停止和失能*/
    ret_stop_status = Motor_Stop(p_motor);
    ret_disable_status = Motor_Disable(p_motor);
    /*无果，再强制停止*/
    if (ret_disable_status != MOTOR_STATUS_OK || ret_stop_status != MOTOR_STATUS_OK)
    {
         (void)Motor_ForceSafeState(p_motor);
         return ACTUATOR_STATUS_ERR_MOTOR;
    }
    return ACTUATOR_STATUS_OK;
}

/**
 * @brief 将执行器标记为故障，并按优先级锁存故障原因。
 * @param[in,out] p_actuator_handle 已完成成员绑定的执行器状态机对象。
 * @param[in] fault 要锁存的故障原因，不能为 ACTUATOR_FAULT_NONE。
 * @pre 调用方已完成所需的硬件安全动作，或已确认无需再次停车。
 * @note 本函数不调用任何 Motor_* 接口，避免故障处理路径重复执行停车。
 *       MOTOR_DRIVER_ERROR 可覆盖先前的故障原因，因为无法确认电机已安全停机。
 */
static void Actuator_LatchFault(ActuatorHandle_t *p_actuator_handle,ActuatorFault_t fault)
{
    p_actuator_handle->current_state = ACTUATOR_STATE_FAULT;

    if ((ACTUATOR_FAULT_NONE == p_actuator_handle->latched_fault) ||
        (ACTUATOR_FAULT_MOTOR_DRIVER_ERROR == fault))
    {
        p_actuator_handle->latched_fault = fault;
    }
}


/**
 * @brief 以安全停机的方式使执行器进入故障状态。
 * @param[in,out] p_actuator_handle 已初始化的执行器状态机对象。
 * @param[in] fault 本次检测到的故障原因，必须不是 ACTUATOR_FAULT_NONE。
 * @return 执行结果。
 * @retval ACTUATOR_STATUS_OK 已安全停机并进入 FAULT，首个故障原因已锁存。
 * @retval ACTUATOR_STATUS_ERR_MOTOR 安全停机失败，已请求强制安全输出并锁存电机驱动故障。
 * @pre p_actuator_handle 及其 motor 成员均有效，且电机驱动已初始化。
 * @note 已锁存首个故障时不覆盖它；安全停机失败时 MOTOR_DRIVER_ERROR 具有最高优先级。
 */
static ActuatorStatus_t Actuator_EnterFault(ActuatorHandle_t *p_actuator_handle,ActuatorFault_t fault)
{
    ActuatorStatus_t ret_actuator_status = ACTUATOR_STATUS_OK;
    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);

    if (ret_actuator_status != ACTUATOR_STATUS_OK)
    {
        Actuator_LatchFault(p_actuator_handle,ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
        return ACTUATOR_STATUS_ERR_MOTOR;
    }
    Actuator_LatchFault(p_actuator_handle,fault);
    return ACTUATOR_STATUS_OK;
}

/**
 * @brief 按已配置的方向和占空比安全启动电机。
 * @param[in,out] p_actuator_handle 已初始化的执行器状态机对象。
 * @param[in] direction 本次运动对应的底层电机方向。
 * @return 执行结果。
 * @retval ACTUATOR_STATUS_OK 电机方向和占空比已预置，并已成功使能。
 * @retval ACTUATOR_STATUS_ERR_MOTOR 任一步 Motor_* 调用失败，已进入 FAULT。
 * @pre 执行器及其电机驱动均已初始化，且调用前电机处于停止、失能状态。
 * @note 本函数只负责底层启动顺序；不修改执行器状态、运动起始时间或编码器监测基准。
 */
static ActuatorStatus_t Actuator_ExecuteSafeStart(ActuatorHandle_t *p_actuator_handle,
                                                  MotorDirection_t direction)
{
    MotorStatus_t motor_status = MOTOR_STATUS_OK;

    /* 失能状态下先完成方向和 PWM 预置，最后才允许功率驱动生效。 */
    motor_status = Motor_SetDirection(p_actuator_handle->motor, direction);//设置电机方向
    if (MOTOR_STATUS_OK != motor_status)
    {
        (void)Actuator_EnterFault(p_actuator_handle, ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
        return ACTUATOR_STATUS_ERR_MOTOR;
    }

    /*设置占空比*/
    motor_status = Motor_SetDuty(p_actuator_handle->motor,
                                 p_actuator_handle->config->run_duty_percent);
    if (MOTOR_STATUS_OK != motor_status)
    {
        (void)Actuator_EnterFault(p_actuator_handle, ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
        return ACTUATOR_STATUS_ERR_MOTOR;
    }
    /*使能电机*/
    motor_status = Motor_Enable(p_actuator_handle->motor);
    if (MOTOR_STATUS_OK != motor_status)
    {
        (void)Actuator_EnterFault(p_actuator_handle, ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
        return ACTUATOR_STATUS_ERR_MOTOR;
    }

    return ACTUATOR_STATUS_OK;
}

ActuatorStatus_t Actuator_Init(ActuatorHandle_t *p_actuator_handle,MotorHandle_t *p_motor,const ActuatorConfig_t *p_config)
{
    /*三个形参首先判空检查*/
    if (NULL == p_actuator_handle ||
        NULL == p_motor ||
        NULL == p_config)
    {
        return ACTUATOR_STATUS_ERR_NULL_POINTER;
    }

    /*判断电机初始化*/
    if (false == p_motor->initialized)
    {
        return ACTUATOR_STATUS_ERR_NOT_INITIALIZED;
    }

    /* 拒绝无法产生有效驱动力，或会使安全监测失效的机构标定。字段含义见 ActuatorConfig_t。 */
    if ((p_config->close_direction == p_config->open_direction) ||
        (p_config->close_direction != MOTOR_DIR_FORWARD && p_config->close_direction != MOTOR_DIR_REVERSE) ||
        (p_config->open_direction != MOTOR_DIR_FORWARD && p_config->open_direction != MOTOR_DIR_REVERSE) ||
        (p_config->run_duty_percent == 0U || p_config->run_duty_percent > 100U) ||
        (p_config->encoder_min_delta == 0U) ||           /* 0 会使“无编码器变化”也被误判为仍在运动。 */
        (p_config->encoder_stall_timeout_ms == 0U) ||    /* 宽限结束后必须保留非零的堵转观察时间。 */
        (p_config->encoder_startup_grace_ms == 0U) ||    /* 启动后必须等待编码器产生首个有效变化，避免立即误报堵转。 */
        (p_config->limit_release_timeout_ms == 0U) ||    /* 离开已触发端点后，原限位必须有非零的释放等待时间。 */
        (p_config->travel_close_timeout_ms == 0U) ||     /* 向关端运动必须有最大行程时间，避免电机无限运行。 */
        (p_config->travel_open_timeout_ms == 0U))        /* 向开端运动必须有最大行程时间，避免电机无限运行。 */
    {
        return ACTUATOR_STATUS_ERR_INVALID_CONFIG;
    }

    /*初始化和赋值各项参数*/
    p_actuator_handle->motor = p_motor;
    p_actuator_handle->config = p_config;
    p_actuator_handle->initialized = false;
    p_actuator_handle->current_state = ACTUATOR_STATE_INIT;
    p_actuator_handle->latched_fault = ACTUATOR_FAULT_NONE;
    p_actuator_handle->motion_start_time_ms = 0U;
    p_actuator_handle->last_encoder_change_time_ms = 0U;
    p_actuator_handle->last_encoder_position = 0;

    /*如果执行安全停止失败*/
    if (Actuator_ExecuteSafeStop(p_motor) != ACTUATOR_STATUS_OK)
    {
        Actuator_LatchFault(p_actuator_handle,ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
        return ACTUATOR_STATUS_ERR_MOTOR;
    }
    /*否则就是正常初始化标记为成功*/
    p_actuator_handle->initialized = true;

    return ACTUATOR_STATUS_OK;

}


ActuatorStatus_t Actuator_Update(ActuatorHandle_t *p_actuator_handle,const ActuatorInput_t *p_input)
{
    ActuatorStatus_t actuator_status = ACTUATOR_STATUS_OK;

    /*形参首先判空检查*/
    if (NULL == p_actuator_handle ||
        NULL == p_input)
    {
        return ACTUATOR_STATUS_ERR_NULL_POINTER;
    }

    /*判断执行器初始化*/
    if (false == p_actuator_handle->initialized)
    {
        return ACTUATOR_STATUS_ERR_NOT_INITIALIZED;
    }



    if ((false == p_input->limit_read_valid) &&
        (ACTUATOR_STATE_FAULT != p_actuator_handle->current_state))
    {
        // 限位快照不可用时，任何健康状态均不得继续执行动作。
        return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_INPUT_INVALID);
    }

    if ((ACTUATOR_STATE_FAULT != p_actuator_handle->current_state) &&
        (true == p_input->limit_stable) &&
        (LIMIT_STATE_CONFLICT == p_input->limit_state))
    {
        // 两个稳定限位同时有效是全局硬件矛盾，优先于所有状态和命令处理。
        return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_CONFLICT);
    }

    // 全局安全条件通过后，再按当前状态处理本周期输入。
    switch (p_actuator_handle->current_state)
    {
        case ACTUATOR_STATE_INIT:
        {
            // 上电阶段只确认初始物理位置；所有动作命令在 INIT 中均被忽略。
            if (false == p_input->limit_stable)
            {
                // 首份去抖完成的限位快照尚未得到，继续保持电机停止、失能。
                return ACTUATOR_STATUS_OK;
            }

            // 用第一份可信限位状态建立初始状态，后续才能响应动作命令。
            switch (p_input->limit_state)
            {
                case LIMIT_STATE_NONE:
                {
                    // 两端均未触发：机构不在已知端点，安全地进入中间停机状态。
                    p_actuator_handle->current_state = ACTUATOR_STATE_IDLE;
                    break;
                }
                case LIMIT_STATE_OPEN_ACTIVE:
                {
                    // 开端已触发：记录端点状态，电机保持停止、失能。
                    p_actuator_handle->current_state = ACTUATOR_STATE_AT_OPEN;
                    break;
                }
                case LIMIT_STATE_CLOSE_ACTIVE:
                {
                    // 关端已触发：记录端点状态，电机保持停止、失能。
                    p_actuator_handle->current_state = ACTUATOR_STATE_AT_CLOSE;
                    break;
                }
                default:
                {
                    // stable 标志与枚举值不一致时，快照已不可信，按限位输入异常处理。
                    return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_INPUT_INVALID);
                }
            }
            break;
        }
        case ACTUATOR_STATE_IDLE:
        {
            // IDLE 的不变量：健康、停止、失能；只允许从此处发起新的单方向运动。
            switch (p_input->command)
            {
                case ACTUATOR_CMD_NONE:
                {
                    // 本周期没有新命令，维持 IDLE 的安全停机状态。
                    break;
                }

                case ACTUATOR_CMD_CLEAR_FAULT:
                {
                    // 当前没有故障需要清除；CLEAR_FAULT 仅在 FAULT 状态下有意义。
                    break;
                }

                case ACTUATOR_CMD_STOP:
                {
                    // 重复 STOP 仍重新请求安全停机，确保外部停止命令可随时加强安全输出。
                    actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != actuator_status)
                    {
                        // 安全停车已尝试 ForceSafeState；此处只锁存，不再重复停车。
                        Actuator_LatchFault(p_actuator_handle,
                                            ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                        return ACTUATOR_STATUS_ERR_MOTOR;
                    }
                    break;
                }

                case ACTUATOR_CMD_OPEN:
                {
                    if (false == p_input->encoder_valid)
                    {
                        // 未确认编码器可用时禁止启动，否则后续无法检测堵转。
                        return Actuator_EnterFault(p_actuator_handle,
                                                   ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);
                    }

                    // 只通过统一安全启动函数设置方向、PWM 与使能，应用层不直接操作硬件。
                    actuator_status = Actuator_ExecuteSafeStart(p_actuator_handle,
                                                                p_actuator_handle->config->open_direction);
                    if (ACTUATOR_STATUS_OK != actuator_status)
                    {
                        // 安全启动函数已完成故障停车和锁存，不能提交 MOVING_OPEN。
                        return actuator_status;
                    }

                    // 以本周期快照作为行程超时与编码器堵转监测的共同起点。
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
                    p_actuator_handle->motion_start_time_ms = p_input->now_ms;
                    // 所有启动与监测基准准备完成后，最后提交“向开端运动”状态。
                    p_actuator_handle->current_state = ACTUATOR_STATE_MOVING_OPEN;
                    break;
                }

                case ACTUATOR_CMD_CLOSE:
                {
                    if (false == p_input->encoder_valid)
                    {
                        // 未确认编码器可用时禁止启动，否则后续无法检测堵转。
                        return Actuator_EnterFault(p_actuator_handle,
                                                   ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);
                    }

                    // 与 OPEN 镜像：使用关方向进行统一安全启动。
                    actuator_status = Actuator_ExecuteSafeStart(p_actuator_handle,
                                                                p_actuator_handle->config->close_direction);
                    if (ACTUATOR_STATUS_OK != actuator_status)
                    {
                        // 安全启动失败时保持其已建立的 FAULT，不能提交 MOVING_CLOSE。
                        return actuator_status;
                    }

                    // 以本周期快照作为关方向运动的超时与堵转监测起点。
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
                    p_actuator_handle->motion_start_time_ms = p_input->now_ms;
                    // 所有启动与监测基准准备完成后，最后提交“向关端运动”状态。
                    p_actuator_handle->current_state = ACTUATOR_STATE_MOVING_CLOSE;
                    break;
                }

                default:
                {
                    // 枚举范围外的命令不能静默忽略；安全停车后锁存命令输入异常。
                    return Actuator_EnterFault(p_actuator_handle,
                                               ACTUATOR_FAULT_COMMAND_INVALID);
                }
            }
            break;
        }
        case ACTUATOR_STATE_FAULT:
        {
            // 故障锁存后绝不因通信恢复或普通命令自动恢复；明天补 CLEAR_FAULT 条件。
            return ACTUATOR_STATUS_OK;
        }
        default:
        {
            // MOVING/AT_* 尚未实现；状态机核心完成前不得下载进行电机联调。
            break;
        }
    }


    return ACTUATOR_STATUS_OK;
}
