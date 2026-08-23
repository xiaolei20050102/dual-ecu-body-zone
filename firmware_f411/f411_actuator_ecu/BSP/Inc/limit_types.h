#ifndef LIMIT_TYPES_H
#define LIMIT_TYPES_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief 限位驱动接口的返回状态。
 */
typedef enum
{
    LIMIT_STATUS_OK = 0,
    LIMIT_STATUS_ERR_NULL_POINTER,
    LIMIT_STATUS_ERR_NOT_INITIALIZED,
    LIMIT_STATUS_ERR_PORT
} LimitStatus_t;

/**
 * @brief 两路限位开关经去抖后的稳定状态。
 */
typedef enum
{
    LIMIT_STATE_NONE = 0,//中间，可操作移动
    LIMIT_STATE_OPEN_ACTIVE,//开到位
    LIMIT_STATE_CLOSE_ACTIVE,//关到位
    LIMIT_STATE_CONFLICT//冲突
} LimitSwitchState_t;

/**
 * @brief 底层采集到的两路限位状态。
 *
 * @note true 表示对应限位已触发。
 */
typedef struct
{
    bool open_active;//开到位限位开关是否被压下
    bool close_active;//关到位限位开关是否被压下
} LimitRawState_t;

/**
 * @brief 提供给控制层的稳定限位状态快照。
 */
typedef struct
{
    LimitSwitchState_t state;
    bool stable_valid;//false 表示“尚未得到第一份完成去抖的限位状态”；此时 state 不能给状态机作为上电判定依据
} LimitSnapshot_t;

#endif /* LIMIT_TYPES_H */
