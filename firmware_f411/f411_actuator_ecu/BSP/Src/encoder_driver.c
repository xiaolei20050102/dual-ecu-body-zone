#include "encoder_driver.h"

#include <stddef.h>

/**
 * @brief 检查底层操作表是否完整。
 * @param[in] port_ops 底层硬件操作表。
 * @return true 表示接口完整。
 */

 static bool Encoder_IsPortOpsValid(const EncoderPortOps_t *port_ops)
{
    if (NULL == port_ops)
    {
        return false;
    }

    if ((NULL == port_ops->init) ||
        (NULL == port_ops->read_raw_count))
    {
        return false;
    }
    return true;
}

EncoderStatus_t Encoder_Init(EncoderHandle_t *encoder,const EncoderPortOps_t *port_ops,void *encoder_context)
{   
    uint16_t raw_count = 0;
    EncoderStatus_t ret_status = ENCODER_STATUS_OK;//函数返回状态
    if (NULL == encoder ||
        NULL == encoder_context)
    {
        return  ENCODER_STATUS_ERR_NULL_POINTER;
    }
    if (false == Encoder_IsPortOpsValid(port_ops)) 
    {
        return ENCODER_STATUS_ERR_PORT;
    }
    encoder->initialized = false;//刚开始非初始化
    ret_status = port_ops->init(encoder_context);//调用底层的初始化函数接口
    if (ENCODER_STATUS_OK != ret_status) 
    {
        return ret_status;
    }

    /*调用底层接口获取没处理的计数*/
    ret_status = port_ops->read_raw_count(encoder_context,&raw_count);
    if (ENCODER_STATUS_OK != ret_status) 
    {
        return ret_status;
    }

    /*初始化encoder句柄的各项值*/
    encoder->last_raw_count = raw_count;
    encoder->position_count = 0;
    encoder->port_ops = port_ops;
    encoder->encoder_context = encoder_context;
    encoder->initialized = true;
    return ENCODER_STATUS_OK;
}


EncoderStatus_t Encoder_Update(EncoderHandle_t *encoder)
{   
    EncoderStatus_t ret_status = ENCODER_STATUS_OK;//函数返回状态
    uint16_t raw_count = 0;//当前编码器计数
    int32_t delta_count = 0;//计数前后差值
    if (NULL == encoder) 
    {
        return ENCODER_STATUS_ERR_NULL_POINTER;
    }

    if (false == encoder->initialized)
    {
        return ENCODER_STATUS_ERR_NOT_INITIALIZED;
    }
    /*调用底层接口拿到最新的编码器计数*/
    ret_status = encoder->port_ops->read_raw_count(encoder->encoder_context,&raw_count);
    if (ENCODER_STATUS_OK != ret_status)
    {
        return ret_status;
    }

    delta_count = (int32_t)raw_count - (int32_t)encoder->last_raw_count;

    /*两个判断是为了防止跨界，处理 16 位原始计数器回绕*/
    if (delta_count > 32767)
    {
        delta_count -= 65536;
    }
    else if (delta_count < -32768)
    {
        delta_count += 65536;
    }    

    if (delta_count > 0 && (encoder->position_count  > INT32_MAX - delta_count))
    {
        return ENCODER_STATUS_ERR_POSITION_OVERFLOW;
    }
    if (delta_count < 0 && (encoder->position_count  < INT32_MIN - delta_count))
    {
        return ENCODER_STATUS_ERR_POSITION_OVERFLOW;
    }    
    encoder->position_count += delta_count;
    encoder->last_raw_count = raw_count;
    return ENCODER_STATUS_OK;
}

EncoderStatus_t Encoder_GetSnapshot(const EncoderHandle_t *encoder,EncoderSnapshot_t *snapshot)
{
    if (NULL == encoder ||
        NULL == snapshot) 
    {
        return ENCODER_STATUS_ERR_NULL_POINTER;
    }

    if (false == encoder->initialized)
    {
        return ENCODER_STATUS_ERR_NOT_INITIALIZED;
    }    

    snapshot->raw_count = encoder->last_raw_count;
    snapshot->position_count = encoder->position_count;

    return ENCODER_STATUS_OK;
}
