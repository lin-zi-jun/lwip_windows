/*********************************************************************************
 * �ļ�����: aliyun_iot_common_log.c
 * ��       ��:
 * ��       ��:
 * ��       ��: 2016-05-30
 * ��       ��:
 * ��       ��:
 * ��       ʷ:
 **********************************************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_platform_stdio.h"
#include "aliyun_iot_platform_persistence.h"
#include "aliyun_iot_platform_pthread.h"
#include "aliyun_iot_platform_timer.h"

IOT_LOG_LEVEL_E g_iotLogLevel = INFO_IOT_LOG;

#ifndef STD_OUT

#define SDK_LOG_CLOSE      0
#define SDK_LOG_OPENNED    1

#define IOT_LOG_FILE_MAX 1024*256
#define IOT_LOG_BUFF_MAX 1024

#define IOT_SDK_LOG_PATH "./"

ALIYUN_IOT_FILE_HANDLE_S g_logFileHandle;
ALIYUN_IOT_MUTEX_S g_mutex;
int g_openSignal = SDK_LOG_CLOSE;

static int sdkLogOpen()
{
    //��־�ļ����� ʱ��+��־��
    char logName[256];

    //����Ѿ�����ֱ���˳�
    if(SDK_LOG_CLOSE != g_openSignal)
    {
        return SUCCESS_RETURN;
    }

    memset(logName,0x0,256);

    ALIYUN_IOT_TIME_TYPE_S nowtimer;
    aliyun_iot_timer_start_clock(&nowtimer);

    ALIYUN_IOT_TIME_TYPE_S starttimer;
    aliyun_iot_timer_init(&starttimer);

    int timeMs = aliyun_iot_timer_interval(&starttimer,&nowtimer);

    aliyun_iot_stdio_snprintf(logName,256,"%s%d_%s",IOT_SDK_LOG_PATH,timeMs,"SDK_log");

    memset(&g_logFileHandle,0x0,sizeof(g_logFileHandle));

    //���ļ�
    if(0 != aliyun_iot_file_open(&g_logFileHandle,logName,CREAT_WR_FALG))
    {
        g_openSignal = SDK_LOG_CLOSE;
        printf("open file of log is error\n");
        return FAIL_RETURN;
    }

    g_openSignal = SDK_LOG_OPENNED;

    return SUCCESS_RETURN;
}

static void sdkLogClose()
{
    if(SDK_LOG_OPENNED != g_openSignal)
    {
        return;
    }

    //�ر��ļ�
    (void)aliyun_iot_file_close(&g_logFileHandle);

    memset(&g_logFileHandle,0x0,sizeof(g_logFileHandle));

    g_openSignal = SDK_LOG_CLOSE;
}

void sdkLog(char* format,char* level,const char* file,int line,const char*function,...)
{
    char buf[IOT_LOG_BUFF_MAX];

    aliyun_iot_mutex_lock(&g_mutex);
    if(0 != sdkLogOpen())
    {
        aliyun_iot_mutex_unlock(&g_mutex);
        return;
    }
    aliyun_iot_mutex_unlock(&g_mutex);

    memset(buf,0x0,IOT_LOG_BUFF_MAX);

    //д��־�ļ�
    aliyun_iot_stdio_snprintf(buf,IOT_LOG_BUFF_MAX-1,"[%s]%s:%d %s()|",level,file,line,function);

    int lens = (int)strlen(buf);

    va_list vArgList;
    va_start (vArgList, function);
    aliyun_iot_stdio_vsnprintf(buf+lens, (IOT_LOG_BUFF_MAX - lens - 1), format, vArgList);
    va_end(vArgList);

    lens = (int)strlen(buf);
    if(IOT_LOG_BUFF_MAX-1 == lens)
    {
        aliyun_iot_stdio_snprintf(buf+lens-1, 2,"\n");
    }
    else
    {
        aliyun_iot_stdio_snprintf(buf+lens, (IOT_LOG_BUFF_MAX - lens - 1),"\n");
    }

    aliyun_iot_file_write(&g_logFileHandle,buf,1,strlen(buf));

    //��־��С�������������ر��ؽ�
    aliyun_iot_mutex_lock(&g_mutex);
    int size = aliyun_iot_file_ftell(&g_logFileHandle);
    if(size < IOT_LOG_FILE_MAX)
    {
        aliyun_iot_mutex_unlock(&g_mutex);
        return;
    }

    //�ؽ���־�ļ�
    sdkLogClose();
    sdkLogOpen();
    aliyun_iot_mutex_unlock(&g_mutex);
}
#endif

void aliyun_iot_common_set_log_level(IOT_LOG_LEVEL_E iotLogLevel)
{
    g_iotLogLevel = iotLogLevel;
}

IOT_LOG_LEVEL_E aliyun_iot_common_get_log_level()
{
    return g_iotLogLevel;
}

void aliyun_iot_common_log_init()
{
#ifndef STD_OUT
    memset(&g_logFileHandle,0x0,sizeof(g_logFileHandle));
    aliyun_iot_mutex_init(&g_mutex);
    g_openSignal = SDK_LOG_CLOSE;
#endif
}

void aliyun_iot_common_log_release()
{
#ifndef STD_OUT
    aliyun_iot_mutex_lock(&g_mutex);
    sdkLogClose();
    g_openSignal = SDK_LOG_CLOSE;
    aliyun_iot_mutex_unlock(&g_mutex);
    aliyun_iot_mutex_destory(&g_mutex);
#endif
}
