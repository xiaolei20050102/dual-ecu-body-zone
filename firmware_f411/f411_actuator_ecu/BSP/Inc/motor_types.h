#ifndef MOTOR_TYPES_H
#define MOTOR_TYPES_H

#include <stdint.h>

/* 电机转动方向 */
/**
 * @brief 电机目标旋转方向枚举。
 */
typedef enum
{
    MOTOR_DIR_FORWARD = 0U,/*正传*/
    MOTOR_DIR_REVERSE/*反转*/
} MotorDirection_t;

/* 电机接口统一返回状态 */
/**
 * @brief 电机模块公共接口的返回状态枚举。
 */
typedef enum
{
    MOTOR_STATUS_OK = 0U,/*状态正确*/
    MOTOR_STATUS_ERR_NULL_POINTER,/*错误，空指针*/
    MOTOR_STATUS_ERR_NOT_INITIALIZED,/*错误，没有初始化*/
    MOTOR_STATUS_ERR_DISABLED,/*错误，没有使能*/
    MOTOR_STATUS_ERR_INVALID_DIRECTION,/*错误，无效的方向*/
    MOTOR_STATUS_ERR_INVALID_DUTY,/*错误，无效的占空比*/
    MOTOR_STATUS_ERR_PORT,/*端口错误，底层硬件适配接口不完整或不可用。*/
    MOTOR_STATUS_ERR_NOT_STOPPED/*错误，没有停止*/
} MotorStatus_t;

#endif /* MOTOR_TYPES_H */
