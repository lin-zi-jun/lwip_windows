/*********************************************************************************
 * 文件名称: aliyun_iot_platform_sem.c
 * 作       者:
 * 版       本:
 * 日       期: 2016-05-30
 * 描       述:
 * 其       它:
 * 历       史:
 **********************************************************************************/

//#include <sys/time.h>
#include <errno.h>
#include "aliyun_iot_platform_sem.h"

/***********************************************************
* 函数名称: aliyun_iot_sem_init
* 描       述: 线程同步信号初始化
* 输入参数: ALIYUN_IOT_SEM_S*semaphore
* 输出参数:
* 返 回  值: 0：成功，
*          FAIL_RETURN：异常
* 说       明:
************************************************************/
int32_t aliyun_iot_sem_init(aliot_platform_sem_t *semaphore)
{
    //int result = 0;
    err_t result;
    
    //result = pthread_mutex_init(&semaphore->lock,NULL);
    result = sys_mutex_new(&(semaphore->lock));
    if(ERR_OK != result)
    {
        return FAIL_RETURN;
    }

    //result = pthread_cond_init(&semaphore->sem,NULL);
    result =  sys_sem_new(&(semaphore->sem), 0);
    if(ERR_OK != result)
    {
        return FAIL_RETURN;
    }

    semaphore->count = 0;

    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_sem_destory
* 描       述: 线程同步信号资源释放
* 输入参数: ALIYUN_IOT_SEM_S*semaphore
* 输出参数:
* 返 回  值: 0：成功，
*          FAIL_RETURN：异常
* 说       明:
************************************************************/
int32_t aliyun_iot_sem_destory(aliot_platform_sem_t *semaphore)
{
    int32_t result = 0;

    sys_mutex_free(&(semaphore->lock));
    sys_sem_free(&(semaphore->sem));

    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_sem_gettimeout
* 描       述: 等待同步信号
* 输入参数: ALIYUN_IOT_SEM_S*semaphore
*          int32_t timeout_ms
* 输出参数:
* 返 回  值: 0：成功，
*          ERROR_NET_TIMEOUT：等待超时，
* 说       明: 等待同步信号，超时退出
************************************************************/
int32_t aliyun_iot_sem_gettimeout(aliot_platform_sem_t*semaphore,uint32_t timeout_ms)
{
    int result = 0;
    int rc = 0;

    sys_mutex_lock(&semaphore->lock);


    while(0 == semaphore->count)
    {
        //result = pthread_cond_timedwait(&semaphore->sem,&semaphore->lock,&timeout);
        sys_arch_sem_wait(&(semaphore->sem), timeout_ms);
        if(SYS_ARCH_TIMEOUT == result)
        {
            //超时则退出等待
            break;
        }
        //else if(0 == result)
        //{
        //    //等到信号
        //    continue;
        //}
        else
        {
            //异常退出
            break;
        }
    }

    if(SYS_ARCH_TIMEOUT != result)
    {
        rc = SUCCESS_RETURN;
        semaphore->count--;
    }
    else //if(ETIMEDOUT == result)
    {
        rc = ERROR_NET_TIMEOUT;
    }
//    else
//    {
//        rc = FAIL_RETURN;
//    }

    //pthread_mutex_unlock(&semaphore->lock);
    sys_mutex_unlock(&semaphore->lock);
    return rc;
}

/***********************************************************
* 函数名称: aliyun_iot_sem_post
* 描       述: 发送同步信号
* 输入参数: ALIYUN_IOT_SEM_S*semaphore
* 输出参数:
* 返 回  值:
* 说       明: 发送同步信号
************************************************************/
int32_t aliyun_iot_sem_post(aliot_platform_sem_t*semaphore)
{
    //pthread_mutex_lock(&semaphore->lock);
    sys_mutex_lock(&semaphore->lock);
    semaphore->count++;
    //pthread_cond_signal(&semaphore->sem);
    sys_sem_signal(&(semaphore->sem));
    sys_mutex_unlock(&semaphore->lock);
    //pthread_mutex_unlock(&semaphore->lock);
    return SUCCESS_RETURN;
}
