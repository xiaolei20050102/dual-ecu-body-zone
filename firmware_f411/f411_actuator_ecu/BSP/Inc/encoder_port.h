#ifndef ENCODER_PORT_H
#define ENCODER_PORT_H
#include <stdint.h>
#include "encoder_types.h"


/*
 * 编码器底层接口操作表。
 * encoder_driver.c 只通过这些函数读取硬件，
 * 不直接依赖 STM32 HAL、TIM2 或 GPIO。
 */
typedef struct 
{
    EncoderStatus_t (*init)(void *encoder_context);
    EncoderStatus_t (*read_raw_count)(void *encoder_context,uint16_t *raw_count);

}EncoderPortOps_t;

#endif /*ENCODER_PORT_H*/
