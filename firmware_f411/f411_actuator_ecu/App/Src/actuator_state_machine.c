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
        p_actuator_handle->current_state = ACTUATOR_STATE_FAULT;//当前状态故障
        p_actuator_handle->latched_fault = ACTUATOR_FAULT_MOTOR_DRIVER_ERROR;
        return ACTUATOR_STATUS_ERR_MOTOR;
    }
    p_actuator_handle->current_state = ACTUATOR_STATE_FAULT;
    if (ACTUATOR_FAULT_NONE == p_actuator_handle->latched_fault)
    {
         p_actuator_handle->latched_fault = fault;
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
        p_actuator_handle->current_state = ACTUATOR_STATE_FAULT;//当前状态故障
        p_actuator_handle->latched_fault = ACTUATOR_FAULT_MOTOR_DRIVER_ERROR;
        return ACTUATOR_STATUS_ERR_MOTOR;
    }
    /*否则就是正常初始化标记为成功*/
    p_actuator_handle->initialized = true;

    return ACTUATOR_STATUS_OK;

}


ActuatorStatus_t Actuator_Update(ActuatorHandle_t *p_actuator_handle,const ActuatorInput_t *p_input)
{
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



    return ACTUATOR_STATUS_OK;
}
