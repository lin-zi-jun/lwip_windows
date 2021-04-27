/*********************************************************************************
 * 文件名称: aliyun_iot_platform_timer.h
 * 作       者:
 * 版       本:
 * 日       期: 2016-05-30
 * 描       述:
 * 其       它:
 * 历       史:
 **********************************************************************************/

#ifndef ALIYUN_IOT_PLATFORM_TIMER_H
#define ALIYUN_IOT_PLATFORM_TIMER_H

//#include <windows.h>
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"
#pragma warning(disable:4255)
typedef struct ALIYUN_IOT_TIME_TYPE
{
    long time;
}ALIYUN_IOT_TIME_TYPE_S;

void aliyun_iot_timer_assignment(INT32 millisecond,ALIYUN_IOT_TIME_TYPE_S *timer);

INT32 aliyun_iot_timer_start_clock(ALIYUN_IOT_TIME_TYPE_S *timer);

INT32 aliyun_iot_timer_spend(ALIYUN_IOT_TIME_TYPE_S *start);

INT32 aliyun_iot_timer_remain(ALIYUN_IOT_TIME_TYPE_S *end);

INT32 aliyun_iot_timer_expired(ALIYUN_IOT_TIME_TYPE_S *timer);

void aliyun_iot_timer_init(ALIYUN_IOT_TIME_TYPE_S* timer);

void aliyun_iot_timer_cutdown(ALIYUN_IOT_TIME_TYPE_S* timer,UINT32 millisecond);

UINT32 aliyun_iot_timer_now();

INT32 aliyun_iot_timer_interval(ALIYUN_IOT_TIME_TYPE_S *start,ALIYUN_IOT_TIME_TYPE_S *end);

#endif

