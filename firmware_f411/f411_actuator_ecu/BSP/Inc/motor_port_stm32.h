#ifndef MOTOR_PORT_STM32_H
#define MOTOR_PORT_STM32_H

#include "stm32f4xx_hal.h"

#include "motor_port.h"


 /*
 * STM32 平台的电机硬件配置。
 * 驱动核心只将该对象作为 void *motor_context 传递，
 * 具体成员仅由 motor_port_stm32.c 使用。
 */
/**
 * @brief STM32 平台的 TB6612 硬件资源配置。
 * @note 本类型只应在 STM32 Port 层使用；驱动核心以 void *motor_context 持有它。
 */
typedef struct
{
    TIM_HandleTypeDef *pwm_timer;
    uint32_t pwm_channel;
    uint32_t pwm_period;

    GPIO_TypeDef *ain1_port;
    uint16_t ain1_pin;

    GPIO_TypeDef *ain2_port;
    uint16_t ain2_pin;

    GPIO_TypeDef *stby_port;
    uint16_t stby_pin;

} MotorPortStm32Config_t;

/* STM32 平台提供给 motor_driver.c 的底层操作表 */
/** @brief STM32 平台的电机 Port 操作表实例。 */
extern const MotorPortOps_t g_motor_port_stm32_ops;

/* 当前 TB6612 A 通道的 STM32 硬件配置 */
/** @brief 当前 TB6612 A 通道的 STM32 硬件配置实例。 */
extern const MotorPortStm32Config_t g_motor_port_stm32_config;

#endif /* MOTOR_PORT_STM32_H */
