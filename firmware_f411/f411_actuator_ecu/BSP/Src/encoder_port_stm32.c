#include "encoder_port_stm32.h"

#include <stddef.h>

#include "tim.h"

/**
 * @brief 初始化 STM32 TIM2 编码器硬件。
 * @param[in] encoder_context STM32 编码器配置对象。
 * @return 初始化结果。
 */

 static EncoderStatus_t EncoderPortStm32_Init(void *encoder_context);

 /**
 * @brief 读取 TIM2 当前原始计数。
 * @param[in] encoder_context STM32 编码器配置对象。
 * @param[out] raw_count 原始计数输出地址。
 * @return 读取结果。
 */
static EncoderStatus_t EncoderPortStm32_ReadRawCount(void *encoder_context,uint16_t *raw_count);

const EncoderPortOps_t g_encoder_port_stm32_ops =
{
    .init = EncoderPortStm32_Init,
    .read_raw_count = EncoderPortStm32_ReadRawCount
};

const EncoderPortStm32Config_t g_encoder_port_stm32_config = 
{
    .timer = &htim2
};

static EncoderStatus_t EncoderPortStm32_Init(void *encoder_context)
{   
    EncoderPortStm32Config_t *config = NULL;
    /*形参判空*/
    if (NULL == encoder_context)
    {
        return ENCODER_STATUS_ERR_NULL_POINTER;
    }
    
    config = (EncoderPortStm32Config_t *)encoder_context;//转化为STM32底层的配置
    /*判断里面的timer参数是否有效*/
    if (NULL == config->timer)
    {
        return ENCODER_STATUS_ERR_PORT;
    }
    /*开启TIM2的所有通道*/
    if (HAL_OK != HAL_TIM_Encoder_Start(config->timer,TIM_CHANNEL_ALL))
    {
        return ENCODER_STATUS_ERR_PORT;
    }
    /*将计数重置为0*/
    __HAL_TIM_SET_COUNTER(config->timer,0U);
    return ENCODER_STATUS_OK;
}

static EncoderStatus_t EncoderPortStm32_ReadRawCount(void *encoder_context,uint16_t *raw_count)
{
    EncoderPortStm32Config_t *config = NULL;
    /*形参判空*/
    if (NULL == encoder_context ||
        NULL == raw_count)
    {
        return ENCODER_STATUS_ERR_NULL_POINTER;
    }

    config = (EncoderPortStm32Config_t *)encoder_context;//转化为STM32底层的配置
    /*判断里面的timer参数是否有效*/
    if (NULL == config->timer)
    {
        return ENCODER_STATUS_ERR_PORT;
    }
    *raw_count = (uint16_t)__HAL_TIM_GET_COUNTER(config->timer);

    return ENCODER_STATUS_OK;
}
