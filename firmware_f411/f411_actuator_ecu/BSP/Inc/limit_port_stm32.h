#ifndef LIMIT_PORT_STM32_H
#define LIMIT_PORT_STM32_H

#include "stm32f4xx_hal.h"

#include "limit_port.h"

/**
 * @brief STM32 平台的双限位硬件资源配置。
 *
 * @note 驱动核心仅将该对象作为 void *limit_context 持有，
 *       具体 GPIO 成员只允许在 STM32 Port 层使用。
 */
typedef struct
{
    GPIO_TypeDef *open_port;
    uint16_t open_pin;

    GPIO_TypeDef *close_port;
    uint16_t close_pin;
} LimitPortStm32Config_t;

/**
 * @brief STM32 平台提供给 limit_driver 的底层操作表实例。
 */
extern const LimitPortOps_t g_limit_port_stm32_ops;

/**
 * @brief 当前双限位开关的 STM32 硬件配置实例。
 */
extern const LimitPortStm32Config_t g_limit_port_stm32_config;

#endif /* LIMIT_PORT_STM32_H */
