#include "aliyun_iot_platform_threadsync.h"

INT32 aliyun_iot_sem_init(ALIYUN_IOT_SEM_S *pSemIoT)
{
    err_t ret;
    if(pSemIoT == NULL) 
    {
        return -1;
    }
    
    ret =  sys_sem_new(&(pSemIoT->pOsEvent), 0);
    //pSemIoT->pOsEvent = OSSemCreate(0);
	if(ret != ERR_OK)
	{
		WRITE_IOT_ERROR_LOG("ucos-II: create sem failed");
		return -1;
	}
	return 0;
}

INT32 aliyun_iot_sem_destory(ALIYUN_IOT_SEM_S *pSemIoT)
{
        if(pSemIoT == NULL)
        {
            return -1;
        }
        sys_sem_free(&(pSemIoT->pOsEvent));
        
	return 0;
}

INT32 aliyun_iot_sem_gettimeout(ALIYUN_IOT_SEM_S *pSemIoT, UINT32 timeout_ms)
{

      if(pSemIoT == NULL)
        {
            return -1;
        }
      
        sys_arch_sem_wait(&(pSemIoT->pOsEvent), timeout_ms);

	return 0;

}

INT32 aliyun_iot_sem_post(ALIYUN_IOT_SEM_S *pSemIoT)
{
        if(pSemIoT == NULL)
        {
            return -1;
        }
        
        sys_sem_signal(&(pSemIoT->pOsEvent));
        
	return 0;
}
