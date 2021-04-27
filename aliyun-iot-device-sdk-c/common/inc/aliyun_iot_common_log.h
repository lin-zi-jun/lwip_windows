/*********************************************************************************
 * �ļ�����: aliyun_iot_common_log.h
 * ��       ��:
 * ��       ��:
 * ��       ��: 2016-05-30
 * ��       ��:
 * ��       ��:
 * ��       ʷ:
 **********************************************************************************/
#ifndef ALIYUN_IOT_COMMON_LOG_H
#define ALIYUN_IOT_COMMON_LOG_H

#include <stdio.h>
#include <stdlib.h>
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"
#define STD_OUT
#pragma warning(disable:4255)
//��־���������
typedef enum IOT_LOG_LEVEL
{
    DEBUG_IOT_LOG = 0,
    INFO_IOT_LOG,
    NOTICE_IOT_LOG,
    WARNING_IOT_LOG,
    ERROR_IOT_LOG,
    OFF_IOT_LOG,        //��־�رձ�־��������ö���������
}IOT_LOG_LEVEL_E;

//ȫ����־�����־�����˱���С�ڵ���Ҫ�����־����ʱ��־�����Ч
extern IOT_LOG_LEVEL_E g_iotLogLevel;

void aliyun_iot_common_log_init();
void aliyun_iot_common_log_release();
void aliyun_iot_common_set_log_level(IOT_LOG_LEVEL_E iotLogLevel);
IOT_LOG_LEVEL_E aliyun_iot_common_get_log_level();
void sdkLog(char* format,char* level,const char* file,int line,const char*function,...);

#ifdef STD_OUT
#define WRITE_IOT_DEBUG_LOG(format, ...) \
{\
    if(g_iotLogLevel <= DEBUG_IOT_LOG)\
    {\
        printf("[debug] %s:%d %s()| "format"\n",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_INFO_LOG(format, ...) \
{\
    if(g_iotLogLevel <= INFO_IOT_LOG)\
    {\
        printf("[info] %s:%d %s()| "format"\n",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_NOTICE_LOG(format, ...) \
{\
    if(g_iotLogLevel <= NOTICE_IOT_LOG)\
    {\
        printf("[notice] %s:%d %s()| "format"\n",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_WARNING_LOG(format, ...) \
{\
    if(g_iotLogLevel <= WARNING_IOT_LOG)\
    {\
        printf("[warning] %s:%d %s()| "format"\n",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_ERROR_LOG(format,...) \
{\
    if(g_iotLogLevel <= ERROR_IOT_LOG)\
    {\
        printf("[error] %s:%d %s()| "format"\n",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#else
#define WRITE_IOT_DEBUG_LOG(format, ...) \
{\
    if(g_iotLogLevel <= DEBUG_IOT_LOG)\
    {\
        sdkLog(format,"debug",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_INFO_LOG(format, ...) \
{\
    if(g_iotLogLevel <= INFO_IOT_LOG)\
    {\
        sdkLog(format,"info",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_NOTICE_LOG(format, ...) \
{\
    if(g_iotLogLevel <= NOTICE_IOT_LOG)\
    {\
        sdkLog(format,"notice",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_WARNING_LOG(format, ...) \
{\
    if(g_iotLogLevel <= WARNING_IOT_LOG)\
    {\
        sdkLog(format,"warning",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}

#define WRITE_IOT_ERROR_LOG(format, ...) \
{\
    if(g_iotLogLevel <= ERROR_IOT_LOG)\
    {\
        sdkLog(format,"error",__FILE__, __LINE__, __FUNCTION__,##__VA_ARGS__);\
    }\
}
#endif

#endif
