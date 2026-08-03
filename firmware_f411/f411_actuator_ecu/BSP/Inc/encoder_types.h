#ifndef ENCODER_TYPES_H
#define ENCODER_TYPES_H

#include <stdint.h>


/**
 * @brief 编码器驱动接口的返回状态。
 */
typedef enum 
{
    ENCODER_STATUS_OK = 0,//状态OK
    ENCODER_STATUS_ERR_NULL_POINTER,//空指针
    ENCODER_STATUS_ERR_NOT_INITIALIZED,//未初始化
    ENCODER_STATUS_ERR_PORT,//接口错误
    ENCODER_STATUS_ERR_POSITION_OVERFLOW,  // 累计位置溢出
}EncoderStatus_t;

/**
 * @brief 编码器累计位置，单位为 TIM2 编码器计数。
 * @note 正负号表示方向；暂不换算为角度、转数或毫米。
 */
typedef int32_t EncoderPosition_t;


/**
 * @brief 编码器位置快照。
 */


typedef struct
{
    EncoderPosition_t position_count;//对外提供，累计的位置值
    uint16_t raw_count;//未处理的计数信息
} EncoderSnapshot_t;



#endif /* ENCODER_TYPES_H */
