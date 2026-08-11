#include "limit_port_stm32.h"
#include "main.h"


 /**
 * @brief 读取限位开关的状态
 * @param[in] limit_context STM32 限位开关配置对象。
 * @param[out] raw_state 原始计状态地址。
 * @return 读取结果。
 */ 
static LimitStatus_t LimitPortStm32_ReadRawState(void *limit_context,LimitRawState_t *raw_state);



static LimitStatus_t LimitPortStm32_ReadRawState(void *limit_context,LimitRawState_t *raw_state)
{
    if (NULL == limit_context ||
        NULL == raw_state)
    
        {
            return LIMIT_STATUS_ERR_NULL_POINTER;
        }

    LimitPortStm32Config_t *config = (LimitPortStm32Config_t*)limit_context;
    /*
    * 限位输入为主动低：
    * GPIO 读取到低电平，代表该方向已经到达限位。
    */
    raw_state->close_active = (GPIO_PIN_RESET == HAL_GPIO_ReadPin(config->close_port,config->close_pin));
    raw_state->open_active = (GPIO_PIN_RESET == HAL_GPIO_ReadPin(config->open_port,config->open_pin));

    return LIMIT_STATUS_OK;
}

/**
 * @brief STM32 平台的双限位底层操作表。
 */
const LimitPortOps_t g_limit_port_stm32_ops = 
{
    .read_raw_state = LimitPortStm32_ReadRawState
};

/**
 * @brief 当前双限位开关的 STM32 硬件配置。
 */
 const LimitPortStm32Config_t g_limit_port_stm32_config =
 {
    .close_pin = LIMIT_CLOSE_N_Pin,
    .close_port = LIMIT_CLOSE_N_GPIO_Port,
    .open_pin = LIMIT_OPEN_N_Pin,
    .open_port = LIMIT_OPEN_N_GPIO_Port,
 };
