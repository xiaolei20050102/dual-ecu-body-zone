#ifndef LIMIT_PORT_H
#define LIMIT_PORT_H

#include "limit_types.h"

/**
 * @brief 限位开关底层操作接口。
 *
 * @note limit_driver 仅通过本接口读取限位状态，
 *       不直接依赖 STM32 HAL、GPIO 或具体引脚。
 */
typedef struct
{
    /**
     * @brief 读取两路限位开关的当前原始状态。
     *
     * @param[in]  limit_context 底层硬件配置对象。
     * @param[out] raw_state     原始限位状态输出地址。
     *
     * @return 读取结果。
     */
    LimitStatus_t (*read_raw_state)(void *limit_context,LimitRawState_t *raw_state);
} LimitPortOps_t;

#endif /* LIMIT_PORT_H */
