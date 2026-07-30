#include "motor_driver.h"

#include <stddef.h>

/*判断这个底层操作表是否有效*/
/**
 * @brief 检查 Port 操作表是否完整。
 * @param[in] port_ops 待检查的 Port 操作表。
 * @return 当全部必需操作非空时返回 true，否则返回 false。
 */
static bool Motor_IsPortOpsValid (const MotorPortOps_t *port_ops)
{
    if (NULL == port_ops)
    {
        return false;
    } 
    if (NULL == port_ops->set_enable ||
        NULL == port_ops->force_safe_state ||
        NULL == port_ops->set_direction ||
        NULL == port_ops->set_duty_percent ||
        NULL == port_ops->init)
    {
        return false;
    }
    return true;
}

MotorStatus_t Motor_Init(MotorHandle_t *motor,const MotorPortOps_t *port_ops,void *motor_context)
{
    MotorStatus_t ret_status = MOTOR_STATUS_OK; 
    /*形参指针判空*/
    if (NULL == motor ||
        NULL == port_ops ||
        NULL == motor_context)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;//返回参数指针空错误
    }

    /*检查port_ops，也就是底层接口（操作表）是否有效*/
    if (false == Motor_IsPortOpsValid(port_ops))
    {
        return MOTOR_STATUS_ERR_PORT;//返回底层接口无效或者不全
    }

    
    /*开始赋值给motor的句柄*/
    motor->port_ops = port_ops;//初始化赋值底层操作表
    motor->motor_context = motor_context;//初始化赋值底层上下文

    motor->initialized = false;
    motor->enabled = false;
    motor->direction = MOTOR_DIR_FORWARD;//这里是默认正传，但是目前初始化都是无效
    motor->duty_percent = 0;//初始化就将pwm设为0不动起来
    
    

    /*这里是底层函数接口去执行真正的底层初始化操作，上方的设置知识软件层的状态*/
    ret_status = motor->port_ops->init(motor->motor_context);
    if (MOTOR_STATUS_OK != ret_status)//底层没初始化成功就返回
    {
        return ret_status;
    }
    /*这里是底层函数接口去执行真正的失能操作，上方的设置知识软件层的状态*/
    motor->port_ops->force_safe_state(motor->motor_context);//无条件失能，pwm为0,安全之后
    motor->initialized = true;
    return MOTOR_STATUS_OK;
}


MotorStatus_t Motor_Enable(MotorHandle_t *motor)
{   
    MotorStatus_t ret_status = MOTOR_STATUS_OK;//返回状态

    if (NULL == motor)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;
    }

    /*没有初始化也不行*/
    if (false == motor->initialized)
    {
        return MOTOR_STATUS_ERR_NOT_INITIALIZED;
    }

    /*
     * 初始化后的硬件方向脚是 0/0。
     * 先写入软件保存的默认方向，再允许 STBY 使能。
     * 此时 PWM 仍为 0，不会让电机转动。
     */
    ret_status = motor->port_ops->set_direction(motor->motor_context, motor->direction);
    if (MOTOR_STATUS_OK != ret_status)
    {
        return ret_status;
    }
    
    
    /*调用接口操作表指向的底层函数*/
    ret_status = motor->port_ops->set_enable(motor->motor_context,true);

    if (MOTOR_STATUS_OK != ret_status)
    {
        return ret_status;
    }
    /*驱动层状态同步为true*/
    motor->enabled = true;

    return MOTOR_STATUS_OK;
}   


MotorStatus_t Motor_SetDirection(MotorHandle_t *motor,MotorDirection_t direction)
{
    MotorStatus_t ret_status = MOTOR_STATUS_OK;//返回状态

    if (NULL == motor)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;
    }
    /*没有初始化也不行*/
    if (false == motor->initialized)
    {
        return MOTOR_STATUS_ERR_NOT_INITIALIZED;
    }    
    /*如果当前在运行，就返回警告*/
    if ((true == motor->enabled) &&
        (motor->duty_percent != 0U))
    {
        return MOTOR_STATUS_ERR_NOT_STOPPED;
    }   
    /*调用转动命令接口处理*/
    ret_status = motor->port_ops->set_direction(motor->motor_context,direction);
    
    if (MOTOR_STATUS_OK != ret_status)
    {
        return ret_status;
    }

     /*更新软件驱动方向状态*/
    motor->direction = direction;
    return MOTOR_STATUS_OK;
}

MotorStatus_t Motor_SetDuty(MotorHandle_t *motor,uint8_t duty_percent)
{
    MotorStatus_t ret_status = MOTOR_STATUS_OK;//返回状态

    if (NULL == motor)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;
    }
    /*没有初始化也不行*/
    if (false == motor->initialized)
    {
        return MOTOR_STATUS_ERR_NOT_INITIALIZED;
    }  
    /*阈值超限也不行*/
    if (duty_percent > 100U)
    {
        return MOTOR_STATUS_ERR_INVALID_DUTY;
    }    
     /*调用底层占空比设置接口处理*/
    ret_status = motor->port_ops->set_duty_percent(motor->motor_context,duty_percent);

    if (MOTOR_STATUS_OK != ret_status)
    {
        return ret_status;
    }    
    /*更新软件驱动占空比状态*/
    motor->duty_percent = duty_percent;
    return MOTOR_STATUS_OK;    
}

MotorStatus_t Motor_Stop(MotorHandle_t *motor)
{
    MotorStatus_t ret_status = MOTOR_STATUS_OK;//返回状态

    if (NULL == motor)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;
    }
    /*没有初始化也不行*/
    if (false == motor->initialized)
    {
        return MOTOR_STATUS_ERR_NOT_INITIALIZED;
    }     
    /*底层接口设置占空比是0*/
    ret_status = motor->port_ops->set_duty_percent(motor->motor_context, 0U);
    if (MOTOR_STATUS_OK != ret_status)
    {
        return ret_status;
    }    
    /*更新软件驱动状态*/
    motor->duty_percent = 0U;
    return MOTOR_STATUS_OK;
}

MotorStatus_t Motor_Disable(MotorHandle_t *motor)
{
    MotorStatus_t ret_status = MOTOR_STATUS_OK;//返回状态

    if (NULL == motor)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;
    }
    /*没有初始化也不行*/
    if (false == motor->initialized)
    {
        return MOTOR_STATUS_ERR_NOT_INITIALIZED;
    }    

    /*底层接口设置占空比是0*/
    ret_status = motor->port_ops->set_duty_percent(motor->motor_context, 0U);
    if (MOTOR_STATUS_OK != ret_status)
    {
        return ret_status;
    }     

    /*底层接口设置stby为0*/
    ret_status = motor->port_ops->set_enable(motor->motor_context,false);
    if (MOTOR_STATUS_OK != ret_status)
    {
        return ret_status;
    }     

    /*更新软件驱动状态*/
    motor->duty_percent = 0U;
    motor->enabled = false;
    return MOTOR_STATUS_OK;
}


MotorStatus_t Motor_ForceSafeState(MotorHandle_t *motor)
{
    if (NULL == motor)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;
    }

    if (false == motor->initialized)
    {
        return MOTOR_STATUS_ERR_NOT_INITIALIZED;
    }

    /*
     * 直接进入平台定义的硬件安全状态：
     * PWM=0、AIN1=0、AIN2=0、STBY=0。
     */
    motor->port_ops->force_safe_state(motor->motor_context);

    motor->duty_percent = 0U;
    motor->enabled = false;

    return MOTOR_STATUS_OK;
}
