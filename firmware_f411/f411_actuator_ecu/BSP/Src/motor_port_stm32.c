#include "motor_port_stm32.h"

#include <stddef.h>

#include "main.h"
#include "tim.h"

/**
 * @brief 初始化 STM32 PWM 端口并启动 PWM 通道。
 * @param[in] motor_context 指向 MotorPortStm32Config_t 的配置上下文。
 * @return Port 初始化结果。
 */
static MotorStatus_t MotorPortStm32_Init(void *motor_context);

/**
 * @brief 设置 TB6612 的 STBY 硬件输出。
 * @param[in] motor_context 指向 MotorPortStm32Config_t 的配置上下文。
 * @param[in] enable true 为使能，false 为失能。
 * @return Port 操作结果。
 */
static MotorStatus_t MotorPortStm32_SetEnable(void *motor_context, bool enable);

/**
 * @brief 设置 TB6612 AIN1/AIN2 的硬件方向输出。
 * @param[in] motor_context 指向 MotorPortStm32Config_t 的配置上下文。
 * @param[in] direction 目标电机方向。
 * @return Port 操作结果。
 */
static MotorStatus_t MotorPortStm32_SetDirection(void *motor_context, MotorDirection_t direction);

/**
 * @brief 将百分比占空比转换为 TIM 比较值并写入 PWM 通道。
 * @param[in] motor_context 指向 MotorPortStm32Config_t 的配置上下文。
 * @param[in] duty_percent 目标占空比，范围为 0~100 (%)。
 * @return Port 操作结果。
 * @note 比较值采用 (ARR + 1) 参与计算，以覆盖 100% 占空比。
 */
static MotorStatus_t MotorPortStm32_SetDutyPercent(void *motor_context, uint8_t duty_percent);

/**
 * @brief 强制 STM32 硬件进入安全状态。
 * @param[in] motor_context 指向 MotorPortStm32Config_t 的配置上下文。
 * @pre motor_context 已在调用路径中完成有效性校验。
 * @note 该函数直接输出 PWM=0、AIN1=0、AIN2=0、STBY=0。
 */
static void MotorPortStm32_ForceSafeState(void *motor_context);

/**
 * @brief 检查 STM32 电机硬件配置是否完整。
 * @param[in] config 待检查的 STM32 电机硬件配置。
 * @return 配置完整时返回 true，否则返回 false。
 */
static bool MotorPortStm32_IsConfigValid(const MotorPortStm32Config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    if ((config->pwm_timer == NULL) ||
        (config->ain1_port == NULL) ||
        (config->ain2_port == NULL) ||
        (config->stby_port == NULL))
    {
        return false;
    }

    return true;
}

/* 当前 TB6612 A 通道的 STM32 硬件映射 */
const MotorPortStm32Config_t g_motor_port_stm32_config =
{
    .pwm_timer = &htim1,//拿到stm32的time句柄地址
    .pwm_channel  = TIM_CHANNEL_1,//通道号
    .pwm_period = 999U,//周期

    .ain1_port = AIN1_GPIO_Port,//AIN1的GOIO口
    .ain1_pin = AIN1_Pin,//AIN1的引脚

    .ain2_port = AIN2_GPIO_Port,
    .ain2_pin = AIN2_Pin,

    .stby_port = STBY_GPIO_Port,
    .stby_pin = STBY_Pin
};

/* 提供给 motor_driver.c 的底层操作表 */
const MotorPortOps_t g_motor_port_stm32_ops =
{   
    /*给全局的操作表赋值底层实现函数接口*/
    .init = MotorPortStm32_Init,
    .set_enable = MotorPortStm32_SetEnable,
    .set_direction = MotorPortStm32_SetDirection,
    .set_duty_percent = MotorPortStm32_SetDutyPercent,
    .force_safe_state = MotorPortStm32_ForceSafeState
};

static MotorStatus_t MotorPortStm32_Init(void *motor_context)
{
    /*将motor_context内容指向*/
    MotorPortStm32Config_t *p_config;

    /*motor_context传参为空，特判空指针错误*/
    if (NULL == motor_context)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;//返回指针空，错误
    }

    /*先判空再做类型转换*/
    p_config = (MotorPortStm32Config_t*)motor_context;

    /*再次检查传入的上下文（motor_context）配置是否合法*/
    if (false == MotorPortStm32_IsConfigValid(p_config))
    {
        return MOTOR_STATUS_ERR_PORT;
    }

    /*强制进入安全模式（失能电机）*/
    MotorPortStm32_ForceSafeState(p_config);


    /*初始化WPM，并且判断是否成功*/
    if (HAL_OK != HAL_TIM_PWM_Start(p_config->pwm_timer, p_config->pwm_channel))
    {
        return MOTOR_STATUS_ERR_PORT;
    }    

     return MOTOR_STATUS_OK;
}

static MotorStatus_t MotorPortStm32_SetEnable(void *motor_context, bool enable)
{
    /*将motor_context内容指向*/
    MotorPortStm32Config_t *p_config;

    /*motor_context传参为空，特判空指针错误*/
    if (NULL == motor_context)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;//返回指针空，错误
    }

    /*先判空再做类型转换*/
    p_config = (MotorPortStm32Config_t*)motor_context;

    /*再次检查传入的上下文（motor_context）配置是否合法*/
    if (false == MotorPortStm32_IsConfigValid(p_config))
    {
        return MOTOR_STATUS_ERR_PORT;
    }

    /*这句看不懂可以回家了*/
    HAL_GPIO_WritePin(p_config->stby_port, p_config->stby_pin,(enable == true) ? GPIO_PIN_SET : GPIO_PIN_RESET);  
    
    return MOTOR_STATUS_OK;
}


static MotorStatus_t MotorPortStm32_SetDirection(void *motor_context, MotorDirection_t direction)
{
    /*将motor_context内容指向*/
    MotorPortStm32Config_t *p_config;

    /*motor_context传参为空，特判空指针错误*/
    if (NULL == motor_context)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;//返回指针空，错误
    }

    /*先判空再做类型转换*/
    p_config = (MotorPortStm32Config_t*)motor_context;

    /*再次检查传入的上下文（motor_context）配置是否合法*/
    if (false == MotorPortStm32_IsConfigValid(p_config))
    {
        return MOTOR_STATUS_ERR_PORT;
    }
    
    /*如果电机时正转命令*/
    if (MOTOR_DIR_FORWARD == direction)   
    {
        HAL_GPIO_WritePin(p_config->ain1_port, p_config->ain1_pin, GPIO_PIN_SET); 
        HAL_GPIO_WritePin(p_config->ain2_port, p_config->ain2_pin, GPIO_PIN_RESET); 
    }
    /*如果电机是反转命令*/
    else if(MOTOR_DIR_REVERSE == direction)
    {
        HAL_GPIO_WritePin(p_config->ain1_port, p_config->ain1_pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(p_config->ain2_port, p_config->ain2_pin, GPIO_PIN_SET);        
    }
    /*其他情况*/
    else
    {
        return MOTOR_STATUS_ERR_INVALID_DIRECTION;//错误，无效的方向命令
    }
    return MOTOR_STATUS_OK;
}

static MotorStatus_t MotorPortStm32_SetDutyPercent(void *motor_context, uint8_t duty_percent)
{

     uint32_t compare_value = 0;
    
    /*将motor_context内容指向*/
    MotorPortStm32Config_t *p_config;

    /*motor_context传参为空，特判空指针错误*/
    if (NULL == motor_context)
    {
        return MOTOR_STATUS_ERR_NULL_POINTER;//返回指针空，错误
    }

    /*先判空再做类型转换*/
    p_config = (MotorPortStm32Config_t*)motor_context;

    /*再次检查传入的上下文（motor_context）配置是否合法*/
    if (false == MotorPortStm32_IsConfigValid(p_config))
    {
        return MOTOR_STATUS_ERR_PORT;
    }    

    /*占空比不可能大于100*/
    if (duty_percent  > 100U)
    {
        return MOTOR_STATUS_ERR_INVALID_DUTY;// 返回占空比无效错误
    }

    /*计算比较值，小于compare_value的都在pwm输出高电平的效果，也就是配置占空比
    这里为什么是duty*ARR/100,而不是duty/100.0*ARR,是因为优化浮点运算*/
    compare_value = ((uint32_t)duty_percent * (p_config->pwm_period + 1U)) / 100U; 
    /*设置HAL的比较值*/
    __HAL_TIM_SET_COMPARE(p_config->pwm_timer, p_config->pwm_channel, compare_value);

    return MOTOR_STATUS_OK;
}


static void MotorPortStm32_ForceSafeState(void *motor_context)
{
    /*将motor_context内容指向*/
    MotorPortStm32Config_t *p_config = (MotorPortStm32Config_t*)motor_context;
    /*直接占空比0，pwm失能*/
    __HAL_TIM_SET_COMPARE(p_config->pwm_timer, p_config->pwm_channel, 0U);

    /*失能*/
    HAL_GPIO_WritePin(p_config->ain1_port, p_config->ain1_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(p_config->ain2_port, p_config->ain2_pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(p_config->stby_port, p_config->stby_pin, GPIO_PIN_RESET);
    
}
