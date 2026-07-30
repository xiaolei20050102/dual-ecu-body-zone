#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "motor_types.h"
#include "motor_port.h"

/*
 * 电机驱动对象句柄。
 * 保存当前电机的软件状态，
 * 并绑定一个具体硬件平台提供的底层操作表。
 */
/**
 * @brief 电机驱动对象。
 * @note 保存运行时状态，并绑定一个具体平台提供的 MotorPortOps_t 操作表。
 */
typedef struct
{
    /*底层的操作表，指向底层提供的函数合集*/
    const MotorPortOps_t *port_ops;
    /*具体硬件配置的上下文*/
    void *motor_context;
     /* 驱动是否已经完成初始化。false 时拒绝正常控制请求。 */
    bool initialized;
    /* 电机驱动是否已使能，即 TB6612 的 STBY 是否处于工作状态。 */
    bool enabled;
     /* 当前保存的电机方向命令。 */
    MotorDirection_t direction;
      /* 当前保存的 PWM 占空比命令，合法范围为 0~100 (%)。 */
    uint8_t duty_percent;

} MotorHandle_t;

/*
 * 绑定驱动对象与具体硬件端口，并进入安全初始状态。
 */
/**
 * @brief  绑定电机驱动对象与底层 Port，并进入安全初始状态。
 * @param[in,out] motor 电机驱动对象。
 * @param[in] port_ops 底层硬件操作表。
 * @param[in] motor_context 底层硬件配置上下文。
 * @return 初始化结果。
 * @retval MOTOR_STATUS_OK 初始化成功，PWM=0、STBY=0。
 * @note 初始化失败时禁止调用其他 Motor_* 控制接口。
 */
MotorStatus_t Motor_Init(MotorHandle_t *motor,const MotorPortOps_t *port_ops,void *motor_context);

/* 使能电机功率驱动 */
/**
 * @brief  使能 TB6612 电机功率驱动。
 * @param[in,out] motor 电机驱动对象。
 * @return 操作结果。
 * @retval MOTOR_STATUS_OK 方向已配置，TB6612 已使能。
 * @note 保留已预置的方向和占空比命令。
 */
MotorStatus_t Motor_Enable(MotorHandle_t *motor);

/* 设置旋转方向；驱动内部会保证换向前先停止 */
/**
 * @brief  设置电机目标旋转方向。
 * @param[in,out] motor 电机驱动对象。
 * @param[in] direction 目标方向。
 * @return 操作结果。
 * @retval MOTOR_STATUS_ERR_NOT_STOPPED 电机已使能且占空比非零，拒绝换向。
 * @note 失能状态下允许预置方向。
 */
MotorStatus_t Motor_SetDirection(MotorHandle_t *motor,MotorDirection_t direction);

/* 设置 PWM 占空比，范围为 0~100 (%) */
/**
 * @brief  设置目标 PWM 占空比。
 * @param[in,out] motor 电机驱动对象。
 * @param[in] duty_percent 目标占空比，范围为 0~100 (%)。
 * @return 操作结果。
 * @retval MOTOR_STATUS_ERR_INVALID_DUTY 占空比超出允许范围。
 * @note 失能状态下允许预置；使能后该命令生效。
 */
MotorStatus_t Motor_SetDuty(MotorHandle_t *motor,uint8_t duty_percent);

/* 安全停止：占空比清零，保持已使能状态 */
/**
 * @brief  停止电机运动。
 * @param[in,out] motor 电机驱动对象。
 * @return 操作结果。
 * @retval MOTOR_STATUS_OK PWM 已清零。
 * @note 保持 TB6612 使能状态和已配置方向不变。
 */
MotorStatus_t Motor_Stop(MotorHandle_t *motor);

/* 安全失能：占空比清零并关闭功率驱动 */
/**
 * @brief  停止电机并失能 TB6612。
 * @param[in,out] motor 电机驱动对象。
 * @return 操作结果。
 * @retval MOTOR_STATUS_OK PWM 已清零，STBY 已拉低。
 */
MotorStatus_t Motor_Disable(MotorHandle_t *motor);

/* 故障路径专用：请求底层无条件进入安全状态 */
/**
 * @brief  强制进入硬件安全状态。
 * @param[in,out] motor 电机驱动对象。
 * @return 操作结果。
 * @retval MOTOR_STATUS_OK PWM=0、AIN1=0、AIN2=0、STBY=0。
 * @note 清除当前占空比并失能；软件方向配置保留。
 */
MotorStatus_t Motor_ForceSafeState(MotorHandle_t *motor);

#endif /* MOTOR_DRIVER_H */
