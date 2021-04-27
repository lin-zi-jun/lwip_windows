/*********************************************************************************
 * 文件名称: aliyun_iot_platform_pthread.c
 * 作       者:
 * 版       本:
 * 日       期: 2016-05-30
 * 描       述:
 * 其       它:
 * 历       史:
 **********************************************************************************/
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_platform_pthread.h"
#include <process.h>
#include <windows.h>

INT32 aliyun_iot_mutex_init(ALIYUN_IOT_MUTEX_S *mutex)
{
    mutex->lock = CreateMutex(NULL,FALSE,NULL);
    if(mutex->lock <= 0)
    {
        return FAIL_RETURN;
    }
    return 0;
}

INT32 aliyun_iot_mutex_lock(ALIYUN_IOT_MUTEX_S *mutex)
{
    return (INT32)WaitForSingleObject(mutex->lock,INFINITE);
}

INT32 aliyun_iot_mutex_unlock(ALIYUN_IOT_MUTEX_S *mutex)
{
    return (INT32)ReleaseMutex(mutex->lock);
}

INT32 aliyun_iot_mutex_destory(ALIYUN_IOT_MUTEX_S *mutex)
{
    CloseHandle(mutex->lock);
    return SUCCESS_RETURN;
}

INT32 aliyun_iot_pthread_create(ALIYUN_IOT_PTHREAD_S* handle,void*(*func)(void*),void *arg,ALIYUN_IOT_PTHREAD_PARAM_S*param)
{
    unsigned ( __stdcall *ThreadFun )( void * );
	ThreadFun = (unsigned(__stdcall*)(void*))(func);

    handle->threadID = (HANDLE)_beginthreadex(NULL, 0, ThreadFun, arg, 0, NULL);
	if (handle->threadID < 0)
	{
		return FAIL_RETURN;
	}
    return SUCCESS_RETURN;
}

INT32 aliyun_iot_pthread_cancel(ALIYUN_IOT_PTHREAD_S*threadID)
{
    if(threadID->threadID != 0)
    {
        TerminateThread(threadID->threadID,0);
        CloseHandle(threadID->threadID);
    }

    return SUCCESS_RETURN;
}

INT32 aliyun_iot_pthread_taskdelay(int MsToDelay)
{
	Sleep(MsToDelay);
	return  0;
}

INT32 aliyun_iot_pthread_setname(char* name)
{
    return SUCCESS_RETURN;
}

