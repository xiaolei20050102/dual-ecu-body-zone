#ifndef LIMIT_DRIVER_H
#define LIMIT_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "limit_types.h"
#include "limit_port.h"


/**
 * @brief 限位开关驱动对象。
 *
 * @note 保存去抖过程中的候选状态与稳定状态，
 *       并绑定一个具体平台的底层操作表。
 */

typedef struct {
    const LimitPortOps_t *port_ops;
    void *limit_context;
    bool initialized;
    LimitRawState_t candidate_raw_state;/*未处理数据候选等待处理消抖*/
    uint8_t candidate_sample_count;/*候选值确认次数计数*/
    LimitSwitchState_t stable_state;/*稳定之后的状态，可用上报给控制层*/
}LimitHandle_t;


/**
 * @brief 绑定限位驱动对象与底层 Port，并采集初始限位状态。
 *
 * @param[in,out] limit         限位驱动对象。
 * @param[in]     port_ops      底层硬件操作表。
 * @param[in]     limit_context 底层硬件配置上下文。
 *
 * @return 初始化结果。
 *
 * @pre port_ops 及其 read_raw_state 成员不能为空。
 */
LimitStatus_t Limit_Init(LimitHandle_t *limit,const LimitPortOps_t *port_ops,void *limit_context);

/**
 * @brief 周期读取限位状态并执行软件去抖。
 *
 * @param[in,out] limit 限位驱动对象。
 *
 * @return 更新结果。
 *
 * @note 由 ControlTask 固定周期调用；本阶段建议每 10 ms 调用一次。
 */
LimitStatus_t Limit_Update(LimitHandle_t *limit);

/**
 * @brief 获取当前稳定的限位状态快照。
 *
 * @param[in]  limit    限位驱动对象。
 * @param[out] snapshot 稳定限位状态输出地址。
 *
 * @return 读取结果。
 */
LimitStatus_t Limit_GetSnapshot(const LimitHandle_t *limit,LimitSnapshot_t *snapshot);
#endif/*LIMIT_DRIVER_H*/
