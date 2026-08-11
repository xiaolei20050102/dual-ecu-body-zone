#include "limit_driver.h"
#include <stddef.h>

#define LIMIT_DEBOUNCE_SAMPLE_COUNT    (3U)/*限位开关抖动采样计数目标值*/

/**
 * @brief 检查限位底层操作表是否完整。
 *
 * @param[in] port_ops 底层操作表。
 *
 * @return true 表示操作表有效。
 */
static bool Limit_IsPortOpsValid(const LimitPortOps_t *port_ops)
{
    if ((NULL == port_ops) || (NULL == port_ops->read_raw_state))
    {
        return false;
    }

    return true;
}

/**
 * @brief 比较两次原始限位采样是否相同。
 *
 * @param[in] left  第一份原始采样。
 * @param[in] right 第二份原始采样。
 *
 * @return true 表示两路限位状态均相同。
 */
static bool Limit_AreRawStatesEqual(const LimitRawState_t *left,const LimitRawState_t *right)
{
    return ((left->open_active == right->open_active) &&
            (left->close_active == right->close_active));
}

/**
 * @brief 将两路原始限位状态转换为统一的逻辑状态。
 *
 * @param[in] raw_state 两路原始限位状态。
 *
 * @return 转换后的限位开关状态。
 */
static LimitSwitchState_t Limit_ConvertRawState(const LimitRawState_t *raw_state)
{
    /*同时触发判定关闭和打开，就是混乱*/
    if (raw_state->open_active && raw_state->close_active)
    {
        return LIMIT_STATE_CONFLICT;
    }

    if (raw_state->open_active)
    {
        return LIMIT_STATE_OPEN_ACTIVE;
    }

    if (raw_state->close_active)
    {
        return LIMIT_STATE_CLOSE_ACTIVE;
    }

    return LIMIT_STATE_NONE;
}

/**
 * @brief 绑定限位驱动对象与底层 Port，并采集初始限位状态。
 *
 * @param[in,out] limit         限位驱动对象。
 * @param[in]     port_ops      底层硬件操作表。
 * @param[in]     limit_context 底层硬件配置上下文。
 *
 * @return 初始化结果。
 */
LimitStatus_t Limit_Init(LimitHandle_t *limit,const LimitPortOps_t *port_ops,void *limit_context)
{
    LimitStatus_t status;
    LimitRawState_t raw_state;

    if ((NULL == limit) || (NULL == port_ops) || (NULL == limit_context))
    {
        return LIMIT_STATUS_ERR_NULL_POINTER;
    }

    if (false == Limit_IsPortOpsValid(port_ops))
    {
        return LIMIT_STATUS_ERR_PORT;
    }

    status = port_ops->read_raw_state(limit_context, &raw_state);
    if (LIMIT_STATUS_OK != status)
    {
        return status;
    }

    limit->port_ops = port_ops;
    limit->limit_context = limit_context;
    limit->candidate_raw_state = raw_state;
    limit->candidate_sample_count = LIMIT_DEBOUNCE_SAMPLE_COUNT;
    limit->stable_state = Limit_ConvertRawState(&raw_state);
    limit->initialized = true;

    return LIMIT_STATUS_OK;
}

LimitStatus_t Limit_Update(LimitHandle_t *limit)
{
    LimitStatus_t status;
    LimitRawState_t raw_state;

    if (NULL == limit)
    {
        return LIMIT_STATUS_ERR_NULL_POINTER;
    }

    if (false == limit->initialized)
    {
        return LIMIT_STATUS_ERR_NOT_INITIALIZED;
    }

    status = limit->port_ops->read_raw_state(limit->limit_context, &raw_state);
    if (LIMIT_STATUS_OK != status)
    {
        return status;
    }

    if (false == Limit_AreRawStatesEqual(&raw_state,&limit->candidate_raw_state))
    {
        /*
         * 采样值发生变化：将其作为新候选值，
         * 重新开始计算连续一致次数。
         */
        limit->candidate_raw_state = raw_state;
        limit->candidate_sample_count = 1U;
    }
    else if (limit->candidate_sample_count < LIMIT_DEBOUNCE_SAMPLE_COUNT)
    {
        limit->candidate_sample_count++;
    }
    else
    {
        /* 候选值已确认，无需继续递增，防止计数溢出。 */
    }

    if (limit->candidate_sample_count >= LIMIT_DEBOUNCE_SAMPLE_COUNT)
    {
        /*达到计数，将当前的候选原始值转化为稳定状态*/
        limit->stable_state = Limit_ConvertRawState(&limit->candidate_raw_state);
    }

    return LIMIT_STATUS_OK;
}

LimitStatus_t Limit_GetSnapshot(const LimitHandle_t *limit,LimitSnapshot_t *snapshot)
{
    if ((NULL == limit) || (NULL == snapshot))
    {
        return LIMIT_STATUS_ERR_NULL_POINTER;
    }

    if (false == limit->initialized)
    {
        return LIMIT_STATUS_ERR_NOT_INITIALIZED;
    }

    snapshot->state = limit->stable_state;

    return LIMIT_STATUS_OK;
}
