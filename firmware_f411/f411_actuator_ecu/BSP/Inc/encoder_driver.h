#ifndef ENCODER_DRIVER_H
#define ENCODER_DRIVER_H
#include <stdbool.h>
#include <stdint.h>
#include "encoder_types.h"
#include "encoder_port.h"

/*
 * 编码器驱动对象。
 * 保存累计位置和上一次原始计数，
 * 并绑定一个具体平台的底层操作表。
 */
typedef struct
{
    const EncoderPortOps_t *port_ops;
    void *encoder_context;

    bool initialized;

    uint16_t last_raw_count;//上一次的原始数据
    EncoderPosition_t position_count;//软件累计位置，处理了正负方向和计数器回绕
} EncoderHandle_t;


/**
 * @brief 绑定编码器驱动对象与底层 Port，并读取初始原始计数。
 * @param[in,out] encoder 编码器驱动对象。
 * @param[in] port_ops 底层硬件操作表。
 * @param[in] encoder_context 底层硬件配置上下文。
 * @return 初始化结果。
 */
EncoderStatus_t Encoder_Init(EncoderHandle_t *encoder,const EncoderPortOps_t *port_ops,void *encoder_context);


/**
 * @brief 读取新的原始计数并更新累计位置。
 * @param[in,out] encoder 编码器驱动对象。
 * @return 更新结果。
 * @note 应周期性调用；本阶段不计算速度。
 */
EncoderStatus_t Encoder_Update(EncoderHandle_t *encoder);

/**
 * @brief 获取当前编码器位置快照。
 * @param[in] encoder 编码器驱动对象。
 * @param[out] snapshot 位置快照输出地址。
 * @return 读取结果。
 */
EncoderStatus_t Encoder_GetSnapshot(const EncoderHandle_t *encoder,EncoderSnapshot_t *snapshot);
#endif/*ENCODER_DRIVER_H*/
