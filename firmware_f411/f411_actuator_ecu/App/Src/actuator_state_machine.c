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
    ActuatorStatus_t ret_actuator_status = ACTUATOR_STATUS_OK;

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
        // 这周期没能读到限位状态，不知道机构是否已到端点，所以先停车并报错
        return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_INPUT_INVALID);
    }

    if ((ACTUATOR_STATE_FAULT != p_actuator_handle->current_state) &&
        (true == p_input->limit_stable) &&
        (LIMIT_STATE_CONFLICT == p_input->limit_state))
    {
        // 开和关限位同时有效，说明接线或机构异常，先停车并报错
        return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_CONFLICT);
    }

    // 前面先处理所有状态都必须响应的故障，通过后才根据当前状态做事
    switch (p_actuator_handle->current_state)
    {
        case ACTUATOR_STATE_INIT:
        {
            // 刚上电时不允许电机动作，只通过限位器判断机构目前在哪里
            if (false == p_input->limit_stable)
            {
                // 限位器还在去抖，当前结果不可信，继续等待
                return ACTUATOR_STATUS_OK;
            }

            // 限位状态稳定后，再决定初始时是中间、开端还是关端
            switch (p_input->limit_state)
            {
                case LIMIT_STATE_NONE:
                {
                    // 两个限位都没按下，说明不在两端，电机保持停止并进入 IDLE
                    p_actuator_handle->current_state = ACTUATOR_STATE_IDLE;
                    break;
                }
                case LIMIT_STATE_OPEN_ACTIVE:
                {
                    // 开限位已按下，说明机构在开端，电机保持停止
                    p_actuator_handle->current_state = ACTUATOR_STATE_AT_OPEN;
                    break;
                }
                case LIMIT_STATE_CLOSE_ACTIVE:
                {
                    // 关限位已按下，说明机构在关端，电机保持停止
                    p_actuator_handle->current_state = ACTUATOR_STATE_AT_CLOSE;
                    break;
                }
                default:
                {
                    // 限位状态不是预期的几种值，不能相信这份输入，按限位异常处理
                    return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_INPUT_INVALID);
                }
            }
            break;
        }
        case ACTUATOR_STATE_IDLE:
        {
            // IDLE 表示机构健康且电机已停止失能，只能从这里开始新的动作
            switch (p_input->command)
            {
                case ACTUATOR_CMD_NONE:
                {
                    // 本周期没有命令，电机继续保持停止
                    break;
                }

                case ACTUATOR_CMD_CLEAR_FAULT:
                {
                    // 当前没有故障，清故障命令没有作用，继续保持停止
                    break;
                }

                case ACTUATOR_CMD_STOP:
                {
                    // 即使当前已经停了，也再次请求停止和失能，保证 STOP 命令一定生效
                    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // Stop 或 Disable 失败时已经尝试强制关闭硬件，这里只记录故障
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
                        // 编码器这周期不可用，启动后就无法判断电机有没有转，所以不启动
                        return Actuator_EnterFault(p_actuator_handle,
                                                   ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);
                    }

                    // 由安全启动函数依次设置开方向、PWM 和使能，状态机不直接操作硬件
                    ret_actuator_status = Actuator_ExecuteSafeStart(p_actuator_handle,
                                                                p_actuator_handle->config->open_direction);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // 启动失败时安全启动函数已经停车并记录故障，不能改成 MOVING_OPEN
                        return ret_actuator_status;
                    }

                    // 记下刚启动时的位置和时间，后续用来判断有没有转动和是否超时
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
                    p_actuator_handle->motion_start_time_ms = p_input->now_ms;
                    // 电机已启动且监测起点已记好，现在才标记为正在向开端运动
                    p_actuator_handle->current_state = ACTUATOR_STATE_MOVING_OPEN;
                    break;
                }

                case ACTUATOR_CMD_CLOSE:
                {
                    if (false == p_input->encoder_valid)
                    {
                        // 编码器这周期不可用，启动后就无法判断电机有没有转，所以不启动
                        return Actuator_EnterFault(p_actuator_handle,
                                                   ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);
                    }

                    // 与开方向相同的启动步骤，只是这里使用关方向
                    ret_actuator_status = Actuator_ExecuteSafeStart(p_actuator_handle,
                                                                p_actuator_handle->config->close_direction);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // 启动失败时安全启动函数已经停车并记录故障，不能改成 MOVING_CLOSE
                        return ret_actuator_status;
                    }

                    // 记下刚启动时的位置和时间，后续用来判断有没有转动和是否超时
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
                    p_actuator_handle->motion_start_time_ms = p_input->now_ms;
                    // 电机已启动且监测起点已记好，现在才标记为正在向关端运动
                    p_actuator_handle->current_state = ACTUATOR_STATE_MOVING_CLOSE;
                    break;
                }

                default:
                {
                    // 命令值不是已定义的几种，不能当作普通命令处理，停车并报错
                    return Actuator_EnterFault(p_actuator_handle,
                                               ACTUATOR_FAULT_COMMAND_INVALID);
                }
            }
            break;
        }
        case ACTUATOR_STATE_MOVING_OPEN://向‘开’端移动的命令
        {
            /*判断编码器读数是否有效*/
            if (false == p_input->encoder_valid)
            {   
                return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);
            }
            /*限位器稳定情况下是否是到达“开”端*/
            if ((true == p_input->limit_stable) && (LIMIT_STATE_OPEN_ACTIVE == p_input->limit_state))
            {
                ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                if (ACTUATOR_STATUS_OK  != ret_actuator_status)
                {
                    Actuator_LatchFault(p_actuator_handle,
                                        ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                    return ACTUATOR_STATUS_ERR_MOTOR;                    
                }
                /*如果是，当前状态更新为已经到达‘开’端*/
                p_actuator_handle->current_state = ACTUATOR_STATE_AT_OPEN;
                return ACTUATOR_STATUS_OK;
            }


            /*关限位释放超时判定，当前是往开方向，所以要判定相反的关方向的限位器是否健康释放*/
            if ((true == p_input->limit_stable) && (LIMIT_STATE_CLOSE_ACTIVE == p_input->limit_state) &&
                (p_input->now_ms - p_actuator_handle->motion_start_time_ms) >= p_actuator_handle->config->limit_release_timeout_ms)
            {
                return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_CLOSE_STUCK);                
            }


            /*判断是否超过开向总行程时间*/
            if((p_input->now_ms - p_actuator_handle->motion_start_time_ms) >= p_actuator_handle->config->travel_open_timeout_ms)
            {
                return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_TRAVEL_TIMEOUT_OPEN);
            }

            
            /*是否仍在启动宽限；若否，再检查编码器堵转*/
            if ((p_input->now_ms - p_actuator_handle->motion_start_time_ms) <= p_actuator_handle->config->encoder_startup_grace_ms)
            {   /*在宽启动宽限的情况下直接更新数据即可*/
                p_actuator_handle->last_encoder_position = p_input->encoder_position;
                p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
            }
            else
            {   /*如果不在了，就要检查是否真的再动*/
                int64_t difference_value_of_encoder_position =(int64_t)(p_input->encoder_position - (int64_t)p_actuator_handle->last_encoder_position);
                if (difference_value_of_encoder_position < 0) difference_value_of_encoder_position *= -1ll;

                /*如果在动（健康），那么就照样更新数据*/
                if (difference_value_of_encoder_position >= p_actuator_handle->config->encoder_min_delta)
                {
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;                
                }
                else/*否则就判定是否堵转，并且上报*/
                {   
                    /*一旦不动的时间达到判定堵转的阈值就上报堵转*/
                    if ((p_input->now_ms - p_actuator_handle->last_encoder_change_time_ms) >= p_actuator_handle->config->encoder_stall_timeout_ms)
                    {
                        return Actuator_EnterFault(p_actuator_handle, ACTUATOR_FAULT_ENCODER_STALL);
                    }
                }                
            }


            /*命令判断*/
            switch (p_input->command)
            {
                case ACTUATOR_CMD_NONE:
                {
                    /*忽略*/
                    break;
                }
                case ACTUATOR_CMD_OPEN:
                {
                    /*忽略*/
                    break;
                }
                case ACTUATOR_CMD_CLOSE:
                {
                    // 运运行中收到反向 CLOSE，不能直接反转，先停止失能，进入 IDLE，等待下一条 CLOSE，立即请求停止和失能，请求停止和失能，保证 STOP 命令一定生效
                    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // Stop 或 Disable 失败时已经尝试强制关闭硬件，这里只记录故障
                        Actuator_LatchFault(p_actuator_handle,
                                            ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                        return ACTUATOR_STATUS_ERR_MOTOR;
                    }                    
                    p_actuator_handle->current_state = ACTUATOR_STATE_IDLE;
                    break;
                }
                case ACTUATOR_CMD_STOP:
                {
                    // 运行中收到 STOP，立即停止并失能,请求停止和失能，保证 STOP 命令一定生效
                    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // Stop 或 Disable 失败时已经尝试强制关闭硬件，这里只记录故障
                        Actuator_LatchFault(p_actuator_handle,
                                            ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                        return ACTUATOR_STATUS_ERR_MOTOR;
                    }                    
                    p_actuator_handle->current_state = ACTUATOR_STATE_IDLE;
                    break;
                }
                case ACTUATOR_CMD_CLEAR_FAULT:
                {   /*忽略*/
                    break;
                }        
                default:
                {
                    // 命令值不是已定义的几种，不能当作普通命令处理，停车并报错
                    return Actuator_EnterFault(p_actuator_handle,
                                               ACTUATOR_FAULT_COMMAND_INVALID);                    
                }                                        
            }
            return ACTUATOR_STATUS_OK;
        }        
        case ACTUATOR_STATE_MOVING_CLOSE://向“关”端移动的命令
        {
            /*判断编码器读数是否有效*/
            if (false == p_input->encoder_valid)
            {   
                return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);
            }

            /*限位器稳定情况下是否是到达“关”端*/
            if ((true == p_input->limit_stable) && (LIMIT_STATE_CLOSE_ACTIVE == p_input->limit_state))
            {
                ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                if (ACTUATOR_STATUS_OK  != ret_actuator_status)
                {
                    Actuator_LatchFault(p_actuator_handle,
                                        ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                    return ACTUATOR_STATUS_ERR_MOTOR;                    
                }
                /*如果是，当前状态更新为已经到达“关”端*/
                p_actuator_handle->current_state = ACTUATOR_STATE_AT_CLOSE;
                return ACTUATOR_STATUS_OK;
            }

            /*开限位释放超时判定，当前是往关方向，所以要判定原来的开限位是否健康释放*/
            if ((true == p_input->limit_stable) && (LIMIT_STATE_OPEN_ACTIVE == p_input->limit_state) &&
                (p_input->now_ms - p_actuator_handle->motion_start_time_ms) >= p_actuator_handle->config->limit_release_timeout_ms)
            {
                return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_LIMIT_OPEN_STUCK);                
            }

            /*判断是否超过关向总行程时间*/
            if((p_input->now_ms - p_actuator_handle->motion_start_time_ms) >= p_actuator_handle->config->travel_close_timeout_ms)
            {
                return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_TRAVEL_TIMEOUT_CLOSE);
            }

            /*是否仍在启动宽限；若否，再检查编码器堵转*/
            if ((p_input->now_ms - p_actuator_handle->motion_start_time_ms) <= p_actuator_handle->config->encoder_startup_grace_ms)
            {   /*在启动宽限的情况下直接更新数据即可*/
                p_actuator_handle->last_encoder_position = p_input->encoder_position;
                p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
            }
            else
            {   /*如果不在了，就要检查是否真的在动*/
                int64_t difference_value_of_encoder_position =(int64_t)(p_input->encoder_position - (int64_t)p_actuator_handle->last_encoder_position);

                if (difference_value_of_encoder_position < 0) difference_value_of_encoder_position *= -1ll;

                /*如果在动（健康），那么就照样更新数据*/
                if (difference_value_of_encoder_position >= p_actuator_handle->config->encoder_min_delta)
                {
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;                
                }
                else/*否则就判定是否堵转，并且上报*/
                {
                    if ((p_input->now_ms - p_actuator_handle->last_encoder_change_time_ms) >= p_actuator_handle->config->encoder_stall_timeout_ms)
                    {
                        return Actuator_EnterFault(p_actuator_handle, ACTUATOR_FAULT_ENCODER_STALL);
                    }
                }
            }

            /*命令判断*/
            switch (p_input->command)
            {
                case ACTUATOR_CMD_NONE:
                {
                    /*忽略*/
                    break;
                }
                case ACTUATOR_CMD_CLOSE:
                {
                    /*忽略*/
                    break;
                }
                case ACTUATOR_CMD_OPEN:
                {
                    // 运行中收到反向 OPEN，不能直接反转，先停止失能，进入 IDLE，等待下一条 OPEN
                    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // Stop 或 Disable 失败时已经尝试强制关闭硬件，这里只记录故障
                        Actuator_LatchFault(p_actuator_handle,
                                            ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                        return ACTUATOR_STATUS_ERR_MOTOR;
                    }                    
                    p_actuator_handle->current_state = ACTUATOR_STATE_IDLE;
                    break;
                }
                case ACTUATOR_CMD_STOP:
                {
                    // 运行中收到 STOP，立即停止并失能
                    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // Stop 或 Disable 失败时已经尝试强制关闭硬件，这里只记录故障
                        Actuator_LatchFault(p_actuator_handle,
                                            ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                        return ACTUATOR_STATUS_ERR_MOTOR;
                    }                    
                    p_actuator_handle->current_state = ACTUATOR_STATE_IDLE;
                    break;
                }
                case ACTUATOR_CMD_CLEAR_FAULT:
                {   /*忽略*/
                    break;
                }
                default:
                {
                    // 命令值不是已定义的几种，不能当作普通命令处理，停车并报错
                    return Actuator_EnterFault(p_actuator_handle,
                                               ACTUATOR_FAULT_COMMAND_INVALID);                    
                }
            }

            return ACTUATOR_STATUS_OK;
        }
        case ACTUATOR_STATE_AT_OPEN:
        {   
            /*稳定状态下去，判断当前前限位器状态是否和当前所在位置不一致*/
            if ((true == p_input->limit_stable) && (LIMIT_STATE_OPEN_ACTIVE != p_input->limit_state))
            {   /*如果成立直接安全停机+上报错误*/
                return Actuator_EnterFault(p_actuator_handle,
                                           ACTUATOR_FAULT_LIMIT_OPEN_POSITION_LOST);
            }

            /*判断当前状态应对不同命令*/
            switch (p_input->command)
            {
                case ACTUATOR_CMD_NONE:
                {
                    /*保持*/
                    break;
                }
                case ACTUATOR_CMD_CLOSE:
                {   
                    /*动电机前检查编码器是否有效*/
                    if (false == p_input->encoder_valid)
                    {
                        return Actuator_EnterFault(p_actuator_handle,
                                                   ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);                        
                    }
                    /*如果有效就调用安全启动*/
                    ret_actuator_status = Actuator_ExecuteSafeStart(p_actuator_handle,
                                                                 p_actuator_handle->config->close_direction);
                    /*启动失败就返回错误*/
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        return ret_actuator_status;
                    }                             
                    /*启动成功就更新数据*/             
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
                    p_actuator_handle->motion_start_time_ms = p_input->now_ms;        
                    /*状态转移到向’关‘端移动*/
                    p_actuator_handle->current_state = ACTUATOR_STATE_MOVING_CLOSE;
                    break;
                }
                case ACTUATOR_CMD_OPEN:
                {   
                    /*保持*/
                    break;
                }
                case ACTUATOR_CMD_STOP:
                {   
                    // 已经在’开‘端 收到 STOP，立即停止并失能
                    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // Stop 或 Disable 失败时已经尝试强制关闭硬件，这里只记录故障
                        Actuator_LatchFault(p_actuator_handle,
                                            ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                        return ACTUATOR_STATUS_ERR_MOTOR;
                    }                    

                    break;
                }
                case ACTUATOR_CMD_CLEAR_FAULT:
                {   
                    /*保持*/
                    break;
                }
                default:
                {
                   return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_COMMAND_INVALID);
                }
            }
            return ACTUATOR_STATUS_OK;

        }
        case ACTUATOR_STATE_AT_CLOSE:
        {
            /*稳定状态下，判断当前限位器状态是否和当前位置不一致*/
            if ((true == p_input->limit_stable) && (LIMIT_STATE_CLOSE_ACTIVE != p_input->limit_state))
            {   /*如果成立直接安全停机+上报错误*/
                return Actuator_EnterFault(p_actuator_handle,
                                           ACTUATOR_FAULT_LIMIT_CLOSE_POSITION_LOST);
            }

            /*判断当前状态应对不同命令*/
            switch (p_input->command)
            {
                case ACTUATOR_CMD_NONE:
                {
                    /*保持*/
                    break;
                }
                case ACTUATOR_CMD_OPEN:
                {
                    /*电机前检查编码器是否有效*/
                    if (false == p_input->encoder_valid)
                    {
                        return Actuator_EnterFault(p_actuator_handle,
                                                   ACTUATOR_FAULT_ENCODER_UPDATE_ERROR);
                    }
                    /*如果有效就调用安全启动*/
                    ret_actuator_status = Actuator_ExecuteSafeStart(p_actuator_handle,
                                                                    p_actuator_handle->config->open_direction);
                    /*启动失败就返回错误*/
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        return ret_actuator_status;
                    }
                    /*启动成功就更新数据*/
                    p_actuator_handle->last_encoder_position = p_input->encoder_position;
                    p_actuator_handle->last_encoder_change_time_ms = p_input->now_ms;
                    p_actuator_handle->motion_start_time_ms = p_input->now_ms;
                    /*状态转移到向“开”端移动*/
                    p_actuator_handle->current_state = ACTUATOR_STATE_MOVING_OPEN;
                    break;
                }
                case ACTUATOR_CMD_CLOSE:
                {
                    /*保持*/
                    break;
                }
                case ACTUATOR_CMD_STOP:
                {
                    // 已经在“关”端 收到 STOP，立即停止并失能
                    ret_actuator_status = Actuator_ExecuteSafeStop(p_actuator_handle->motor);
                    if (ACTUATOR_STATUS_OK != ret_actuator_status)
                    {
                        // Stop 或 Disable 失败时已经尝试强制关闭硬件，这里只记录故障
                        Actuator_LatchFault(p_actuator_handle,
                                            ACTUATOR_FAULT_MOTOR_DRIVER_ERROR);
                        return ACTUATOR_STATUS_ERR_MOTOR;
                    }

                    break;
                }
                case ACTUATOR_CMD_CLEAR_FAULT:
                {
                    /*保持*/
                    break;
                }
                default:
                {
                   return Actuator_EnterFault(p_actuator_handle,ACTUATOR_FAULT_COMMAND_INVALID);
                }
            }
            return ACTUATOR_STATUS_OK;

        }
        case ACTUATOR_STATE_FAULT:
        {
            // 进入故障后不因普通命令自动恢复，明天再补清故障的条件
            return ACTUATOR_STATUS_OK;
        }
        default:
        {
            // MOVING 和 AT 状态还没写，完成前不能下载到板子上带电机测试
            break;
        }
    }


    return ACTUATOR_STATUS_OK;
}
