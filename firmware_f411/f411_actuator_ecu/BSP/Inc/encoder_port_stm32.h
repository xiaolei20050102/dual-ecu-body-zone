#ifndef ENCODER_PORT_STM32_H
#define ENCODER_PORT_STM32_H
#include "stm32f4xx_hal.h"

#include "encoder_port.h"

/*
 * STM32 平台的编码器硬件配置。
 * 当前项目中，TIM2 已由 CubeMX 配置为 Encoder Mode。
 */
typedef struct 
{
    TIM_HandleTypeDef *timer;

}EncoderPortStm32Config_t;

/* STM32 平台提供给 encoder_driver.c 的底层操作表。 */
extern const EncoderPortOps_t g_encoder_port_stm32_ops;
/* 当前编码器对应的 STM32 硬件配置。 */
extern const EncoderPortStm32Config_t g_encoder_port_stm32_config;
#endif/* ENCODER_PORT_STM32_H */
