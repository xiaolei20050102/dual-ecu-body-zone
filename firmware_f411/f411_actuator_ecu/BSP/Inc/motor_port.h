#ifndef MOTOR_PORT_H
#define MOTOR_PORT_H

#include <stdbool.h>
#include <stdint.h>

#include "motor_types.h"



/*接口合同层*/


/*
 * 电机底层端口操作表。
 * motor_driver.c 只通过这些函数操作硬件，
 * 不直接依赖 STM32 HAL、GPIO 或 TIM。
 *
 * motor_context 指向具体平台的配置对象；
 * 对 STM32 平台，它将指向 TIM/GPIO 引脚配置。
 */
/**
 * @brief 电机硬件端口操作表。
 * @note 驱动核心仅依赖本操作表，不直接依赖具体 MCU、HAL、GPIO 或 TIM。
 */
typedef struct
{
    /** @brief 初始化底层 PWM 与安全默认输出。 */
    MotorStatus_t (*init)(void *motor_context);

    /** @brief 使能或失能电机功率级。 */
    MotorStatus_t (*set_enable)(void *motor_context, bool enable);

    /** @brief 写入硬件方向控制输出。 */
    MotorStatus_t (*set_direction)(void *motor_context,MotorDirection_t direction);

    /** @brief 写入目标 PWM 占空比。 */
    MotorStatus_t (*set_duty_percent)(void *motor_context,uint8_t duty_percent);

    /* 无条件进入硬件安全状态：PWM=0、驱动失能 */
    /** @brief 强制硬件进入安全状态。 */
    void (*force_safe_state)(void *motor_context);

} MotorPortOps_t;

#endif /* MOTOR_PORT_H */
