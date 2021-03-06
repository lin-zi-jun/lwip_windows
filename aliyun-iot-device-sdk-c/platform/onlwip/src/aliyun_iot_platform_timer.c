#include "aliyun_iot_platform_timer.h"

//UINT32 portTICK_RATE_MS = 1000/OS_TICKS_PER_SEC;

UINT32 GetOSTimeTick()
{
	return sys_now();
}

INT32 aliyun_iot_timer_start_clock(ALIYUN_IOT_TIME_TYPE_S *timer)
{
    timer->time = GetOSTimeTick();

    return (INT32)SUCCESS_RETURN;
}

INT32 aliyun_iot_timer_spend(ALIYUN_IOT_TIME_TYPE_S *start)
{
    UINT32 now, res;

    now = GetOSTimeTick();
    res = now - start->time;
    return res;
}

INT32 aliyun_iot_timer_remain(ALIYUN_IOT_TIME_TYPE_S *end)
{
    UINT32 now, res;

    now = GetOSTimeTick();
    res = end->time - now;
    return res;
}

INT32 aliyun_iot_timer_expired(ALIYUN_IOT_TIME_TYPE_S *timer)
{
    UINT32 now = 0;
    INT32 res = 0;
    now = GetOSTimeTick();

    res = timer->time -now;
    return ( res < 0 );
}

void aliyun_iot_timer_init(ALIYUN_IOT_TIME_TYPE_S* timer)
{
    timer->time = 0;
}

void aliyun_iot_timer_assignment(INT32 millisecond,ALIYUN_IOT_TIME_TYPE_S *timer)
{
    timer->time = millisecond;
}

void aliyun_iot_timer_cutdown(ALIYUN_IOT_TIME_TYPE_S* timer,UINT32 millisecond)
{
    UINT32 now = GetOSTimeTick();
    UINT32 interval = millisecond;
    timer->time = now + interval;
}

UINT32 aliyun_iot_timer_now()
{
    return (UINT32) GetOSTimeTick();
}

INT32 aliyun_iot_timer_interval(ALIYUN_IOT_TIME_TYPE_S *start, ALIYUN_IOT_TIME_TYPE_S *end)
{
    return (start->time - end->time);
}


