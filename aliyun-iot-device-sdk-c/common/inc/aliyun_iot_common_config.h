/*********************************************************************************
 * 文件名称: aliyun_iot_common_config.h
 * 作       者:
 * 版       本:
 * 日       期: 2016-05-30
 * 描       述:
 * 其       它:
 * 历       史:
 **********************************************************************************/
#ifndef ALIYUN_IOT_COMMON_CONFIG_H
#define ALIYUN_IOT_COMMON_CONFIG_H

#include "aliyun_iot_common_datatype.h"

#define IOT_DEVICE_INFO_LEN  64

typedef struct IOT_DEVICE_INFO
{
    INT8  hostName[IOT_DEVICE_INFO_LEN];
    INT8  productKey[IOT_DEVICE_INFO_LEN];
    INT8  productSecret[IOT_DEVICE_INFO_LEN];
    INT8  deviceName[IOT_DEVICE_INFO_LEN];
    INT8  deviceSecret[IOT_DEVICE_INFO_LEN];
}IOT_DEVICE_INFO_S;

extern IOT_DEVICE_INFO_S g_deviceInfo;

#endif
