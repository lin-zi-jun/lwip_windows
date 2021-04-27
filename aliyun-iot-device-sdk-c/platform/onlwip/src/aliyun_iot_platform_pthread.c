#include "aliyun_iot_platform_pthread.h"

#define  IOT_MQTT_TASK_STK_SIZE      3072
#define OS_TASK_TMR_PRIO 10
#define  IOT_MQTT_TASK_PRIO                 (OS_TASK_TMR_PRIO + 22)
#define  IOT_MQTT_MUTEX_PRIO                 (OS_TASK_TMR_PRIO + 15)


static UINT8 taskPri = IOT_MQTT_TASK_PRIO;
static UINT8 mutexPri = IOT_MQTT_MUTEX_PRIO;

INT32 aliyun_iot_mutex_init( ALIYUN_IOT_MUTEX_S *mutex )
{
	UINT8 Err = 0;
	err_t ret;
    if( mutex == NULL )
    {
        IOT_FUNC_EXIT_RC(ERROR_NULL_VALUE);
    }
	
	ret = sys_sem_new(&(mutex->osEvent), 1);
    //mutex->osEvent = OSMutexCreate(mutexPri, &Err);

	if(ERR_OK != ret)
	{
		//WRITE_IOT_ERROR_LOG("��C/OS-II:create mutex failed: %d", Err);
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}

	mutexPri--;
	
	IOT_FUNC_EXIT_RC(SUCCESS_RETURN);

}

INT32 aliyun_iot_mutex_destory( ALIYUN_IOT_MUTEX_S *mutex )
{
	//UINT8 Opt = OS_DEL_NO_PEND;
	//UINT8 Err = 0;
	err_t ret;
    if( mutex == NULL )
    {
    	IOT_FUNC_EXIT_RC(ERROR_NULL_VALUE);
    }
	
	sys_sem_free(&(mutex->osEvent));
    
    //OSMutexDel(mutex->osEvent, Opt, &Err);

	//if(OS_ERR_NONE != Err)
	if(0)
	{
		//WRITE_IOT_ERROR_LOG("��C/OS-II:destory mutex failed: %d", Err);
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}

    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}

INT32 aliyun_iot_mutex_lock( ALIYUN_IOT_MUTEX_S *mutex )
{
	UINT32 Timeout = 0;
    UINT8 Err = 0;
    if( mutex == NULL )
    {
        IOT_FUNC_EXIT_RC(ERROR_NULL_VALUE);
    }
	
 

	/*wait forever*/
    //OSMutexPend(mutex->osEvent, Timeout, &Err);
	sys_sem_wait(&(mutex->osEvent));
	//if(OS_ERR_NONE != Err)
	if(0)
	{
		//WRITE_IOT_ERROR_LOG("��C/OS-II:lock mutex failed: %d", Err);
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}

    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}


INT32 aliyun_iot_mutex_unlock( ALIYUN_IOT_MUTEX_S *mutex )
{
	UINT8 ret = 0;
    if( mutex == NULL )
    {
        IOT_FUNC_EXIT_RC(ERROR_NULL_VALUE);
    }

	//ret = OSMutexPost(mutex->osEvent);
	sys_sem_signal(&(mutex->osEvent));
	//if(OS_ERR_NONE != ret)
	if(0)
	{
		WRITE_IOT_ERROR_LOG("��C/OS-II:unlock mutex failed: %d", ret);
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}
	
    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}

INT32 aliyun_iot_pthread_create(ALIYUN_IOT_PTHREAD_S* handle, void*(*func)(void*), void *arg, ALIYUN_IOT_PTHREAD_PARAM_S*param)
{
	UINT8 ret = 0;
	UINT8 taskName[16];
/*	
	OS_STK  *IoTAppStartTaskStack = (OS_STK *)aliyun_iot_memory_malloc(IOT_MQTT_TASK_STK_SIZE);
	if(NULL == IoTAppStartTaskStack)
	{
		WRITE_IOT_ERROR_LOG("��C/OS-II:malloc IoTAppTaskStack failed");
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}
	
    ret = OSTaskCreate((void(*)(void*))(func), arg, &IoTAppStartTaskStack[IOT_MQTT_TASK_STK_SIZE -1], taskPri); 
	if(0 != ret)
	{
		WRITE_IOT_ERROR_LOG("��C/OS-II:TaskCreate failed: %d", ret);
		aliyun_iot_memory_free(IoTAppStartTaskStack);
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}
*/
	snprintf(taskName, 16, "iot_thread_%d", taskPri);
	handle->osSTK = sys_thread_new(taskName,  func,  arg, IOT_MQTT_TASK_STK_SIZE, taskPri);
	handle->taskPri = taskPri;	
	
	taskPri++;
    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}

INT32 aliyun_iot_pthread_cancel(ALIYUN_IOT_PTHREAD_S*handle)
{
	UINT8 ret = 0;
    return ret;
    /*
    ret = OSTaskDel(handle->taskPri);
	
	if(0 != ret)
	{
		WRITE_IOT_ERROR_LOG("��C/OS-II:OSTaskDel failed: %d", ret);
		if(NULL != handle->osSTK)
		{
			aliyun_iot_memory_free(handle->osSTK);
		}
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}

	
	if(NULL != handle->osSTK)
	{
		aliyun_iot_memory_free(handle->osSTK);
	}
	
	IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
     */
}

//UINT32 tickRateMS = 1000/OS_TICKS_PER_SEC;

INT32 aliyun_iot_pthread_taskdelay(int MsToDelay)
{
	//UINT32 msToTick = (MsToDelay/tickRateMS == 0) ? 1: (MsToDelay/tickRateMS);

	//OSTimeDly(msToTick);
	sys_msleep(MsToDelay);
	
	IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}

INT32 aliyun_iot_pthread_setname(char* name)
{
    return SUCCESS_RETURN;
}


