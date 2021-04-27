/*********************************************************************************
 * 文件名称: aliyun_iot_auth.c
 * 版       本:
 * 日       期: 2016-05-30
 * 描       述:
 * 其       它:
 * 历       史:
 **********************************************************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "aliyun_iot_platform_stdio.h"
#include "aliyun_iot_platform_network.h"
#include "aliyun_iot_platform_persistence.h"
#include "aliyun_iot_platform_memory.h"
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_common_config.h"
#include "aliyun_iot_common_base64.h"
#include "aliyun_iot_common_json.h"
#include "aliyun_iot_common_md5.h"
#include "aliyun_iot_common_httpclient.h"
#include "aliyun_iot_common_hmac.h"
#include "aliyun_iot_auth.h"
#include "aliyun_iot_mqtt_internal.h"

#define  MQTT_DEVICE_NOT_EXSIT_ERRORCODE "InvalidDevice"

/***********************************************************
* 全局变量: g_userInfo
* 描       述: 用户信息
* 说       明:
************************************************************/
ALIYUN_IOT_USER_INFO_S g_userInfo;

/***********************************************************
* 全局变量: g_authInfo
* 描       述: 鉴权信息内存指针
* 说       明: 初始化时malloc得到authinfo内存保存地址在此变量中
************************************************************/
AUTH_INFO_S *g_authInfo = NULL;

/***********************************************************
* 函数名称: aliyun_iot_set_auth_state
* 描       述: 设置鉴权模式
* 输入参数: USER_AUTH_STATE_E authState
* 输出参数: VOID
* 返 回  值: VOID
* 说       明: EVERY_TIME_AUTH    每次调用aliyun_iot_auth接口都会重新获取证书鉴权
*           FIRST_CONNECT_AUTH 只在设备第一次上线时鉴权，以后使用记录的鉴权信息
************************************************************/
void aliyun_iot_set_auth_state(USER_AUTH_STATE_E authState)
{
    g_authInfo->authStateInfo.authState = authState;
}

/***********************************************************
* 函数名称: aliyun_iot_get_response
* 描       述: 鉴权响应信息的解析
* 输入参数: const INT8* at    httpclient获取的鉴权信息
* 输出参数: AUTH_INFO_S *info 解析的鉴权结果
* 返 回  值: 0：成功  -1：失败
* 说       明: httpclient获取的鉴权响应信息进行Json解析，获得鉴权内容
************************************************************/
static INT32 aliyun_iot_get_response(const INT8* at, AUTH_INFO_S *info)
{
	cJSON *result = NULL;
	UINT8 *key = NULL;
	INT32 out_len = 0;
    cJSON *jsonObj = aliyun_iot_common_json_parse(at);
    if(NULL == jsonObj)
    {
        WRITE_IOT_ERROR_LOG("get NULL pointer of json!");
        return FAIL_RETURN;
    }

    result = jsonObj->child;
    while (result)
    {
        if (!strcmp(result->string, "pubkey"))
        {
            key = (UINT8*)result->valuestring;

            if((UINT32)strlen((INT8*)key)>KEY_LEN_MAX)
            {
			    aliyun_iot_common_json_delete(jsonObj);
                WRITE_IOT_ERROR_LOG("the length of string more than memory!");
                return FAIL_RETURN;
            }

            
            if(SUCCESS_RETURN != aliyun_iot_common_base64decode(key, (UINT32)strlen((INT8*)key), (UINT32)KEY_LEN_MAX,(UINT8*)info->pubkey,(UINT32*)&out_len))
            {
			    aliyun_iot_common_json_delete(jsonObj);
                WRITE_IOT_ERROR_LOG("run aliyun_iot_common_base64decode error!");
                return FAIL_RETURN;
            }

            info->pubkey[out_len] = 0;
        }
        else if (!strcmp(result->string, "pkVersion"))
        {
            info->pkVersion = atoi(result->valuestring);
        }
        else if (!strcmp(result->string, "servers"))
        {
            memset(info->servers,0x0,HOST_ADDRESS_LEN);
            strncpy(info->servers,result->valuestring,HOST_ADDRESS_LEN);
        }
        else if (!strcmp(result->string, "sign"))
        {
            memset(info->sign,0x0,SIGN_STRING_LEN);
            strncpy(info->sign,result->valuestring,SIGN_STRING_LEN);
        }
        else if(!strcmp(result->string, "success"))
        {
            WRITE_IOT_DEBUG_LOG("success: %s",result->valuestring);
        }
        else if(!strcmp(result->string, "deviceId"))
        {
            memset(info->deviceId,0x0,DEVICE_ID_LEN);
            WRITE_IOT_DEBUG_LOG("deviceId: %s",result->valuestring);
            strncpy(info->deviceId,result->valuestring,DEVICE_ID_LEN);
        }
        else if(!strcmp(result->string, "errorCode"))
        {
            memset(info->errCode,0x0,ERROR_CODE_LEN_MAX);
            strncpy(info->errCode,result->valuestring,ERROR_CODE_LEN_MAX);
        	WRITE_IOT_ERROR_LOG("auth failed: errorCode: %s",result->valuestring);
        }
        result = result->next;
    }

    //删除资源
    aliyun_iot_common_json_delete(jsonObj);

    return SUCCESS_RETURN;
}

/****************************************************************************************
* 函数名称: aliyun_iot_make_sign
* 描       述: 生成post信息签名的函数
* 输入参数: SIGN_DATA_TYPE_E signDataType
* 输出参数: INT8 *signMethod 签名方法
*           INT8 *sign       签名结果
* 返 回  值: 0：成功  -1：失败
* 说       明: MD5原数据为productSecret+deviceName+productKey+signMethod+deviceSecret
*          hmacmd5/hmacsha1原数据为deviceName+productKey+signMethod，密钥为productSecret+deviceSecret
******************************************************************************************/
static INT32 aliyun_iot_make_sign(SIGN_DATA_TYPE_E signDataType,INT8 *signMethod,SERVER_PARAM_TYPE_E pullType,INT8 *sign)
{
	//注意栈空间大小
    INT8 data[STRING_LEN];
    INT8 signKey[SIGN_KEY_LEN];
	UINT32 lens = 0;
    if(NULL == sign || NULL == signMethod)
    {
        WRITE_IOT_ERROR_LOG("pointer is null!");
        return FAIL_RETURN;
    }

    memset(data,0x0,STRING_LEN);
    memset(signKey,0x0,SIGN_KEY_LEN);

    //判断数据长度是否超出STRING_LEN限制
    lens = strlen(g_deviceInfo.productSecret)+strlen(g_deviceInfo.productKey)+strlen(g_deviceInfo.deviceName)+strlen(g_deviceInfo.deviceSecret)+ strlen("productKey deviceName signMethodprotocolmqtt");
    if(lens >STRING_LEN)
    {
        WRITE_IOT_ERROR_LOG("length of data exceeds the maximum value!");
        return FAIL_RETURN;
    }

    //根据指定算法类型对签名进行处理
    if(MD5_SIGN_TYPE == signDataType)
    {
        //MD5算法生成签名值
        if(NETWORK_SERVER_PARAM == pullType)
        {
            aliyun_iot_stdio_snprintf(data, STRING_LEN, "%sdeviceName%sproductKey%sprotocolmqttresFlagipsdkVersion%ssignMethod%s%s", g_deviceInfo.productSecret,g_deviceInfo.deviceName,g_deviceInfo.productKey,MQTT_SDK_VERSION,signMethod,g_deviceInfo.deviceSecret);

        }
        else if(CERT_SERVER_PARAM == pullType)
        {
            aliyun_iot_stdio_snprintf(data, STRING_LEN, "%sdeviceName%sproductKey%sprotocolmqttresFlagcertsdkVersion%ssignMethod%s%s", g_deviceInfo.productSecret,g_deviceInfo.deviceName,g_deviceInfo.productKey,MQTT_SDK_VERSION,signMethod,g_deviceInfo.deviceSecret);
        }
        else
        {
            aliyun_iot_stdio_snprintf(data, STRING_LEN, "%sdeviceName%sproductKey%sprotocolmqttsdkVersion%ssignMethod%s%s", g_deviceInfo.productSecret,g_deviceInfo.deviceName,g_deviceInfo.productKey,MQTT_SDK_VERSION,signMethod,g_deviceInfo.deviceSecret);
        }
        aliyun_iot_common_md5(data,strlen(data),sign);
    }
    else
    {
        aliyun_iot_stdio_snprintf(signKey,SIGN_KEY_LEN,"%s%s",g_deviceInfo.productSecret,g_deviceInfo.deviceSecret);

        if(NETWORK_SERVER_PARAM == pullType)
        {
            aliyun_iot_stdio_snprintf(data, STRING_LEN, "deviceName%sproductKey%sprotocolmqttresFlagipsdkVersion%ssignMethod%s", g_deviceInfo.deviceName,g_deviceInfo.productKey,MQTT_SDK_VERSION,signMethod);
        }
        else if(CERT_SERVER_PARAM == pullType)
        {
            aliyun_iot_stdio_snprintf(data, STRING_LEN, "deviceName%sproductKey%sprotocolmqttresFlagcertsdkVersion%ssignMethod%s", g_deviceInfo.deviceName,g_deviceInfo.productKey,MQTT_SDK_VERSION,signMethod);
        }
        else
        {
            aliyun_iot_stdio_snprintf(data, STRING_LEN, "deviceName%sproductKey%sprotocolmqttsdkVersion%ssignMethod%s", g_deviceInfo.deviceName,g_deviceInfo.productKey,MQTT_SDK_VERSION,signMethod);
        }

        if(HMAC_SHA1_SIGN_TYPE == signDataType)
        {
            aliyun_iot_common_hmac_sha1(data,strlen(data), sign, signKey, strlen(signKey));
        }
        else
        {
            aliyun_iot_common_hmac_md5(data,strlen(data), sign, signKey, strlen(signKey));
        }
    }

    return SUCCESS_RETURN;
}


/***********************************************************
* 函数名称: aliyun_iot_get_server_param
* 描       述: 获取数据服务器参数
* 输入参数: SIGN_DATA_TYPE_E signDataType 签名方法
*           SERVER_PARAM_TYPE_E pullType 获取的参数类型
* 输出参数: AUTH_INFO_S *info 鉴权信息
* 返 回  值: 0：成功 非0：失败
* 说       明: 通过http向服务端获取鉴权信息，如果有文件系统，则使用已经保存的鉴权信息，不再从网络获取
************************************************************/
INT32 pull_server_param(SIGN_DATA_TYPE_E signDataType,SERVER_PARAM_TYPE_E pullType,AUTH_INFO_S *info)
{
    //以下为鉴权获取证书
    INT8 *page = "/iot/auth";
	//响应数据大小
    int responseSize = STRING_RESPOSE;
	httpclient_t client;
    httpclient_data_t client_data;
	INT8 *response_buf = NULL;
	IOT_RETURN_CODES_E result;
    //注意栈空间大小
    INT8 postString[STRING_LEN];
    INT8 sign[SIGN_STRING_LEN];
    INT8 signMethod[SIGN_METHOD_LEN];
    memset(sign,0x0,SIGN_STRING_LEN);
    memset(postString,0x0,STRING_LEN);
    memset(signMethod,0x0,SIGN_METHOD_LEN);
	printf("[ZZZZ] in pull_server_param\n");
    //根据指定算法类型对签名进行处理
    if(MD5_SIGN_TYPE == signDataType)
    {
        //MD5算法生成签名值
        aliyun_iot_stdio_snprintf(signMethod,SIGN_METHOD_LEN,"%s","MD5");
    }
    else if(HMAC_SHA1_SIGN_TYPE == signDataType)
    {
        aliyun_iot_stdio_snprintf(signMethod,SIGN_METHOD_LEN,"%s","HmacSHA1");
    }
    else
    {
        aliyun_iot_stdio_snprintf(signMethod,SIGN_METHOD_LEN,"%s","HmacMD5");
    }
	printf("[ZZZZ] in aliyun_iot_stdio_snprintf out\n");
    //获取签名的值
    if(SUCCESS_RETURN != aliyun_iot_make_sign(signDataType,signMethod,pullType,sign))
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_make_sign error!");
        return FAIL_RETURN;
    }

    printf("[ZZZZ]  aliyun_iot_make_sign out\n");

    //拼接url内容
    if(NETWORK_SERVER_PARAM == pullType)
    {
        //拉取IP信息
        sprintf(postString, "http://%s%s?&sign=%s&productKey=%s&deviceName=%s&protocol=mqtt&sdkVersion=%s&signMethod=%s&resFlag=ip",g_deviceInfo.hostName, page,sign, g_deviceInfo.productKey, g_deviceInfo.deviceName,MQTT_SDK_VERSION,signMethod);
        responseSize = STRING_IP_RESPOSE;
    }
    else if(CERT_SERVER_PARAM == pullType)
    {
        //拉取证书信息
        sprintf(postString, "http://%s%s?&sign=%s&productKey=%s&deviceName=%s&protocol=mqtt&sdkVersion=%s&signMethod=%s&resFlag=cert",g_deviceInfo.hostName, page,sign, g_deviceInfo.productKey, g_deviceInfo.deviceName,MQTT_SDK_VERSION,signMethod);
        responseSize = STRING_RESPOSE;
    }
    else
    {
        //拉取所有信息
        sprintf(postString, "http://%s%s?&sign=%s&productKey=%s&deviceName=%s&protocol=mqtt&sdkVersion=%s&signMethod=%s",g_deviceInfo.hostName, page,sign, g_deviceInfo.productKey, g_deviceInfo.deviceName,MQTT_SDK_VERSION,signMethod);
        responseSize = STRING_RESPOSE;
    }

    WRITE_IOT_DEBUG_LOG("hostName: %s",g_deviceInfo.hostName);
    WRITE_IOT_DEBUG_LOG("page: %s",page);
    WRITE_IOT_DEBUG_LOG("sign: %s",sign);
    WRITE_IOT_DEBUG_LOG("signMethod: %s",signMethod);
    WRITE_IOT_DEBUG_LOG("g_deviceInfo.productKey: %s",g_deviceInfo.productKey);
    WRITE_IOT_DEBUG_LOG("g_deviceInfo.deviceName: %s", g_deviceInfo.deviceName);
    WRITE_IOT_DEBUG_LOG("postString: %s",postString);
    WRITE_IOT_DEBUG_LOG("mqtt sdk version:%s", MQTT_SDK_VERSION);

    
    memset(&client,0x0,sizeof(httpclient_t));
    memset(&client_data,0x0,sizeof(httpclient_data_t));

    response_buf = aliyun_iot_memory_malloc(responseSize);
    if (response_buf == NULL)
    {
        WRITE_IOT_ERROR_LOG("malloc http response buf failed!");
        return FAIL_RETURN;
    }
    memset(response_buf,0x0,responseSize);

    client_data.response_buf = response_buf;
    client_data.response_buf_len = responseSize;
    printf("[ZZZZ]  aliyun_iot_common_post before\n");
    result = aliyun_iot_common_post(&client, postString, HTTP_PORT, &client_data);
    if(SUCCESS_RETURN != result)
    {
        aliyun_iot_memory_free(response_buf);
        WRITE_IOT_ERROR_LOG("run aliyun_iot_common_post error!");
        return result;
    }
	printf("[ZZZZ]  aliyun_iot_get_response before\n");
    memset(info->errCode,0x0,ERROR_CODE_LEN_MAX);
    result = aliyun_iot_get_response(response_buf,info);
    if(SUCCESS_RETURN != result)
    {
        aliyun_iot_memory_free(response_buf);
        WRITE_IOT_ERROR_LOG("run aliyun_iot_get_response error!");
        return result;
    }
	printf("[ZZZZ]  aliyun_iot_get_response after\n");
    aliyun_iot_memory_free(response_buf);

    WRITE_IOT_DEBUG_LOG("pkVersion: %d",info->pkVersion);
    WRITE_IOT_NOTICE_LOG("servers: %s",info->servers);
    WRITE_IOT_DEBUG_LOG("sign: %s",info->sign);

    if(0 != info->errCode[0])
    {
        WRITE_IOT_ERROR_LOG("run pull_server_param() error!errcode = %s",info->errCode);

        //拉取IP时返回错误码设备被删除
        if(strncmp(info->errCode,MQTT_DEVICE_NOT_EXSIT_ERRORCODE,sizeof(MQTT_DEVICE_NOT_EXSIT_ERRORCODE)))
        {
            return ERROR_DEVICE_NOT_EXSIT;
        }

        return FAIL_RETURN;
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_read_authInfo
* 描       述: 读取鉴权信息
* 输入参数: AUTH_INFO_S *info 鉴权信息
* 输出参数:
* 返 回  值: 0：成功  -1：失败
* 说       明: 将鉴权信息记录在文件中
************************************************************/
static INT32 aliyun_iot_read_authInfo(AUTH_INFO_S *authInfo)
{
    //如果文件存在则读文件记录信息
    ALIYUN_IOT_FILE_HANDLE_S handle;
    ALIYUN_IOT_FILE_FLAG_E flags = RD_FLAG;
	INT32 lens;
	INT8 version[SIGN_STRING_LEN];
	INT8 *pointer = NULL;
    memset(&handle,0x0,sizeof(handle));

    //有证书文件存在则将证书文件存放到AUTH_INFO_S的内存中
    if(SUCCESS_RETURN != aliyun_iot_file_open(&handle,authInfo->trustStorePath,flags))
    {
        WRITE_IOT_ERROR_LOG("open the file public key is error!");
        return FAIL_RETURN;
    }

    lens = aliyun_iot_file_read(&handle, authInfo->pubkey, KEY_LEN_MAX,1);
    if (lens < 0)
    {
        aliyun_iot_file_close(&handle);
        WRITE_IOT_ERROR_LOG("read the file is error!lens = %d",lens);
        return FAIL_RETURN;
    }

    aliyun_iot_file_close(&handle);

    memset(version,0x0,SIGN_STRING_LEN);
    if(SUCCESS_RETURN!= aliyun_iot_file_open(&handle,authInfo->otherInfoFilePath,flags))
    {
        WRITE_IOT_ERROR_LOG("open the file public key is error!");
        return FAIL_RETURN;
    }

    if (SUCCESS_RETURN!=aliyun_iot_file_fgets(&handle, version, SIGN_STRING_LEN))
    {
        aliyun_iot_file_close(&handle);
        WRITE_IOT_ERROR_LOG("write the file is error!lens = %d",lens);
        return FAIL_RETURN;
    }

    pointer = strchr(version,'\n');
    if(pointer != NULL)
    {
        *pointer = 0;
    }
    authInfo->pkVersion = atoi(version);

    if (SUCCESS_RETURN != aliyun_iot_file_fgets(&handle,authInfo->sign, SIGN_STRING_LEN))
    {
        aliyun_iot_file_close(&handle);
        WRITE_IOT_ERROR_LOG("write the file is error!");
        return FAIL_RETURN;
    }

    pointer = strchr(authInfo->sign,'\n');
    if(pointer != NULL)
    {
        *pointer = 0;
    }

    if (SUCCESS_RETURN != aliyun_iot_file_fgets(&handle,authInfo->deviceId, DEVICE_ID_LEN))
    {
        aliyun_iot_file_close(&handle);
        WRITE_IOT_ERROR_LOG("write the file is error!");
        return FAIL_RETURN;
    }

    pointer = strchr(authInfo->deviceId,'\n');
    if(pointer != NULL)
    {
        *pointer = 0;
    }

    WRITE_IOT_NOTICE_LOG("use the file of auth!");
    aliyun_iot_file_close(&handle);
    return SUCCESS_RETURN;

}

/***********************************************************
* 函数名称: aliyun_iot_record_authInfo
* 描       述: 记录鉴权信息
* 输入参数: AUTH_INFO_S *info 鉴权信息
* 输出参数:
* 返 回  值: 0：成功  -1：失败
* 说       明: 将鉴权信息记录在文件中
************************************************************/
static INT32 aliyun_iot_record_authInfo(AUTH_INFO_S *info)
{
	INT32 lens;
	INT8 version[SIGN_STRING_LEN];
	ALIYUN_IOT_FILE_HANDLE_S handle;
	ALIYUN_IOT_FILE_FLAG_E flags = CREAT_RDWR_FLAG;
    //生成证书路径
    WRITE_IOT_INFO_LOG("trustStorePath:%s",info->trustStorePath);
    WRITE_IOT_INFO_LOG("otherInfoFilePath:%s",info->otherInfoFilePath);

    
    memset(&handle,0x0,sizeof(handle));
    

    //记录公钥证书文件
    if(SUCCESS_RETURN!= aliyun_iot_file_open(&handle,info->trustStorePath,flags))
    {
        WRITE_IOT_ERROR_LOG("open the file public key is error!");
        return FAIL_RETURN;
    }

    lens = aliyun_iot_file_write(&handle, info->pubkey, strlen(info->pubkey),1);
    if (lens != 1)
    {
        aliyun_iot_file_close(&handle);
        (void)aliyun_iot_file_delete(info->trustStorePath);
        WRITE_IOT_ERROR_LOG("write the file is error! lens=%d",lens);
        return FAIL_RETURN;
    }
    aliyun_iot_file_close(&handle);

    //记录其它鉴权信息
    if(SUCCESS_RETURN!= aliyun_iot_file_open(&handle,info->otherInfoFilePath,flags))
    {
        (void)aliyun_iot_file_delete(info->trustStorePath);
        WRITE_IOT_ERROR_LOG("open the file raw public key is error!");
        return FAIL_RETURN;
    }

    
    memset(version,0x0,SIGN_STRING_LEN);
    sprintf(version,"%d\n",info->pkVersion);
    lens = aliyun_iot_file_fputs(&handle,version);
    if (lens<=0)
    {
        aliyun_iot_file_close(&handle);
        (void)aliyun_iot_file_delete(info->trustStorePath);
        (void)aliyun_iot_file_delete(info->otherInfoFilePath);
        WRITE_IOT_ERROR_LOG("write the file is error!");
        return FAIL_RETURN;
    }

    lens = aliyun_iot_file_fputs(&handle,info->authStateInfo.certSign);
    if (lens<=0)
    {
        aliyun_iot_file_close(&handle);
        (void)aliyun_iot_file_delete(info->trustStorePath);
        (void)aliyun_iot_file_delete(info->otherInfoFilePath);
        WRITE_IOT_ERROR_LOG("write the file is error!");
        return FAIL_RETURN;
    }

    lens = aliyun_iot_file_fputs(&handle,"\n");
    if(lens <= 0)
    {
        aliyun_iot_file_close(&handle);
        (void)aliyun_iot_file_delete(info->trustStorePath);
        (void)aliyun_iot_file_delete(info->otherInfoFilePath);
        WRITE_IOT_ERROR_LOG("write the file is error!");
        return FAIL_RETURN;
    }

    lens = aliyun_iot_file_fputs(&handle,info->deviceId);
    if (lens<=0)
    {
        aliyun_iot_file_close(&handle);
        (void)aliyun_iot_file_delete(info->trustStorePath);
        (void)aliyun_iot_file_delete(info->otherInfoFilePath);
        WRITE_IOT_ERROR_LOG("write the file is error!");
        return FAIL_RETURN;
    }

    aliyun_iot_file_close(&handle);

    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_set_device_info
* 描       述: 设置设备信息
* 输入参数: IOT_DEVICEINFO_SHADOW_S*deviceInfo
* 输出参数: VOID
* 返 回  值: 0：成功  -1：失败
* 说       明: 将在aliyun注册的设备信息设置到SDK中的设备变量中
************************************************************/
INT32 aliyun_iot_set_device_info(IOT_DEVICEINFO_SHADOW_S*deviceInfo)
{
    if(NULL == deviceInfo)
    {
        WRITE_IOT_ERROR_LOG("input pointer is NULL");
        return FAIL_RETURN;
    }

    if(deviceInfo->productKey == NULL || deviceInfo->productSecret == NULL || deviceInfo->deviceName == NULL || deviceInfo->deviceSecret == NULL || deviceInfo->hostName == NULL)
    {
        WRITE_IOT_ERROR_LOG("the content of device info is NULL");
        return FAIL_RETURN;
    }

    WRITE_IOT_DEBUG_LOG("start to set device info!");
    memset(&g_deviceInfo,0x0,sizeof(g_deviceInfo));

    strncpy(g_deviceInfo.productKey,deviceInfo->productKey,IOT_DEVICE_INFO_LEN);
    strncpy(g_deviceInfo.productSecret,deviceInfo->productSecret,IOT_DEVICE_INFO_LEN);
    strncpy(g_deviceInfo.deviceName,deviceInfo->deviceName,IOT_DEVICE_INFO_LEN);
    strncpy(g_deviceInfo.deviceSecret,deviceInfo->deviceSecret,IOT_DEVICE_INFO_LEN);
    strncpy(g_deviceInfo.hostName,deviceInfo->hostName,IOT_DEVICE_INFO_LEN);

    WRITE_IOT_DEBUG_LOG("set device info success!");

    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_auth_init
* 描       述: auth初始化函数
* 输入参数: VOID
* 输出参数: VOID
* 返 回  值: 0 成功，-1 失败
* 说       明: 初始化日志级别，设备信息，分配authinfo内存，
*           鉴权信息文件的保存路径
************************************************************/
INT32 aliyun_iot_auth_init()
{
    aliyun_iot_common_set_log_level(INFO_IOT_LOG);
    aliyun_iot_common_log_init();

    memset(&g_deviceInfo,0x0,sizeof(IOT_DEVICE_INFO_S));

    if(NULL==g_authInfo)
    {
        g_authInfo = (AUTH_INFO_S*)aliyun_iot_memory_malloc(sizeof(AUTH_INFO_S));
        if(NULL == g_authInfo)
        {
            return FAIL_RETURN;
        }
    }

    memset(g_authInfo,0x0,sizeof(AUTH_INFO_S));
    strncpy(g_authInfo->trustStorePath,"./publicKey.crt",strlen("./publicKey.crt"));
    strncpy(g_authInfo->otherInfoFilePath,"./auth.info",strlen("./auth.info"));

    WRITE_IOT_NOTICE_LOG("auth init success!");
    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_auth_release
* 描       述: auth释放函数
* 输入参数: VOID
* 输出参数: VOID
* 返 回  值: 0:成功 -1:失败
* 说      明: 释放authInfo内存
************************************************************/
INT32 aliyun_iot_auth_release()
{
    if(NULL != g_authInfo)
    {
        aliyun_iot_memory_free(g_authInfo);
    }

    g_authInfo = NULL;
    WRITE_IOT_NOTICE_LOG("auth release!");
    aliyun_iot_common_log_release();
    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_verify_certificate
* 描       述: 证书验证函数
* 输入参数: AUTH_INFO_S *authInfo
*          SIGN_DATA_TYPE_E signDataType
* 输出参数: VOID
* 返 回  值: 0:成功 -1:失败
* 说       明: 验证证书是否合法
************************************************************/
INT32 aliyun_iot_verify_certificate(AUTH_INFO_S *authInfo,SERVER_PARAM_TYPE_E pullType,SIGN_DATA_TYPE_E signDataType)
{
    //注意栈空间大小
    INT8 data[STRING_RESPOSE];
	INT8 sign[SIGN_STRING_LEN];
	INT8 certSign[SIGN_STRING_LEN];
	INT8 signKey[SIGN_KEY_LEN];
	INT8 pubkeyEncode[KEY_LEN_MAX];
	UINT32 outLen = 0;
    UINT32 lens = 0;
    memset(data,0x0,STRING_RESPOSE);

    
    memset(sign,0x0,SIGN_STRING_LEN);

    
    memset(certSign,0x0,SIGN_STRING_LEN);

    
    memset(signKey,0x0,SIGN_KEY_LEN);

    
    memset(pubkeyEncode,0x0,KEY_LEN_MAX);



    if(ALL_SERVER_PARAM == pullType)
    {
        //证书base64编码
        if (SUCCESS_RETURN != aliyun_iot_common_base64encode((UINT8 *)authInfo->pubkey, (UINT32)strlen((INT8*)authInfo->pubkey), (UINT32)KEY_LEN_MAX,(UINT8*)pubkeyEncode,&outLen))
        {
            WRITE_IOT_ERROR_LOG("base64encode pubkey is error!");
            return FAIL_RETURN;
        }

        //判断数据长度是否超出STRING_LEN限制
        lens = strlen(g_deviceInfo.productSecret)+strlen(pubkeyEncode)+strlen(authInfo->servers)+strlen(authInfo->deviceId)+strlen(g_deviceInfo.deviceSecret) +strlen("   pkVersionpubkeyserver");
        if(lens > STRING_RESPOSE)
        {
            WRITE_IOT_ERROR_LOG("length of data exceeds the maximum value!");
            return FAIL_RETURN;
        }

        //根据指定算法类型对签名进行处理
        if(MD5_SIGN_TYPE == signDataType)
        {
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "%sdeviceId%spkVersion%dpubkey%sservers%s%s", g_deviceInfo.productSecret, authInfo->deviceId,authInfo->pkVersion, pubkeyEncode, authInfo->servers,g_deviceInfo.deviceSecret);
            aliyun_iot_common_md5(data,strlen(data),sign);

            //同时计算出只有证书信息的签名
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "%sdeviceId%spkVersion%dpubkey%s%s", g_deviceInfo.productSecret, authInfo->deviceId,authInfo->pkVersion, pubkeyEncode,g_deviceInfo.deviceSecret);
            aliyun_iot_common_md5(data,strlen(data),certSign);
        }
        else
        {
            aliyun_iot_stdio_snprintf(signKey,SIGN_KEY_LEN,"%s%s",g_deviceInfo.productSecret,g_deviceInfo.deviceSecret);
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "deviceId%spkVersion%dpubkey%sservers%s", authInfo->deviceId,authInfo->pkVersion, pubkeyEncode, authInfo->servers);

            if(HMAC_SHA1_SIGN_TYPE == signDataType)
            {
                aliyun_iot_common_hmac_sha1(data,strlen(data), sign, signKey, strlen(signKey));
            }
            else
            {
                aliyun_iot_common_hmac_md5(data,strlen(data), sign, signKey, strlen(signKey));
            }

            //同时计算出只有证书信息的签名
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "deviceId%spkVersion%dpubkey%s", authInfo->deviceId,authInfo->pkVersion, pubkeyEncode);
            if(HMAC_SHA1_SIGN_TYPE == signDataType)
            {
                aliyun_iot_common_hmac_sha1(data,strlen(data), certSign, signKey, strlen(signKey));
            }
            else
            {
                aliyun_iot_common_hmac_md5(data,strlen(data), certSign, signKey, strlen(signKey));
            }
        }

        memset(authInfo->authStateInfo.certSign,0x0,SIGN_STRING_LEN);
        strncpy(authInfo->authStateInfo.certSign,certSign,sizeof(certSign));

        WRITE_IOT_DEBUG_LOG("sign from ALL_SERVER_PARAM");
    }
    else if(CERT_SERVER_PARAM == pullType)
    {
        //证书base64编码
        if (SUCCESS_RETURN != aliyun_iot_common_base64encode((UINT8 *)authInfo->pubkey, (UINT32)strlen((INT8*)authInfo->pubkey), (UINT32)KEY_LEN_MAX,(UINT8*)pubkeyEncode,&outLen))
        {
            WRITE_IOT_ERROR_LOG("base64encode pubkey is error!");
            return FAIL_RETURN;
        }

        lens = strlen(g_deviceInfo.productSecret)+strlen(pubkeyEncode)+strlen(authInfo->deviceId)+strlen(g_deviceInfo.deviceSecret) +strlen("   pkVersionpubkey");
        if(lens > STRING_RESPOSE)
        {
            WRITE_IOT_ERROR_LOG("length of data exceeds the maximum value!");
            return FAIL_RETURN;
        }

        //根据指定算法类型对签名进行处理
        if(MD5_SIGN_TYPE == signDataType)
        {
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "%sdeviceId%spkVersion%dpubkey%s%s", g_deviceInfo.productSecret, authInfo->deviceId,authInfo->pkVersion, pubkeyEncode,g_deviceInfo.deviceSecret);
            aliyun_iot_common_md5(data,strlen(data),sign);
        }
        else
        {
            aliyun_iot_stdio_snprintf(signKey,SIGN_KEY_LEN,"%s%s",g_deviceInfo.productSecret,g_deviceInfo.deviceSecret);
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "deviceId%spkVersion%dpubkey%s", authInfo->deviceId,authInfo->pkVersion, pubkeyEncode);
            if(HMAC_SHA1_SIGN_TYPE == signDataType)
            {
                aliyun_iot_common_hmac_sha1(data,strlen(data), sign, signKey, strlen(signKey));
            }
            else
            {
                aliyun_iot_common_hmac_md5(data,strlen(data), sign, signKey, strlen(signKey));
            }
        }

        WRITE_IOT_DEBUG_LOG("sign from CERT_SERVER_PARAM");
    }
    else
    {
        //判断数据长度是否超出STRING_LEN限制
        lens = strlen(g_deviceInfo.productSecret)+strlen(authInfo->servers)+strlen(g_deviceInfo.deviceSecret) +strlen(authInfo->deviceId) + strlen("   server");
        if(lens > STRING_RESPOSE)
        {
            WRITE_IOT_ERROR_LOG("length of data exceeds the maximum value!");
            return FAIL_RETURN;
        }

        //根据指定算法类型对签名进行处理
        if(MD5_SIGN_TYPE == signDataType)
        {
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "%sdeviceId%sservers%s%s", g_deviceInfo.productSecret, authInfo->deviceId,authInfo->servers,g_deviceInfo.deviceSecret);
            aliyun_iot_common_md5(data,strlen(data),sign);
        }
        else
        {
            aliyun_iot_stdio_snprintf(signKey,SIGN_KEY_LEN,"%s%s",g_deviceInfo.productSecret,g_deviceInfo.deviceSecret);
            aliyun_iot_stdio_snprintf(data, STRING_RESPOSE, "deviceId%sservers%s", authInfo->deviceId,authInfo->servers);
            if(HMAC_SHA1_SIGN_TYPE == signDataType)
            {
                aliyun_iot_common_hmac_sha1(data,strlen(data), sign, signKey, strlen(signKey));
            }
            else
            {
                aliyun_iot_common_hmac_md5(data,strlen(data), sign, signKey, strlen(signKey));
            }
        }

        WRITE_IOT_DEBUG_LOG("sign from NETWORK_SERVER_PARAM");
    }

    WRITE_IOT_DEBUG_LOG("sign:%s",sign);
    WRITE_IOT_DEBUG_LOG("authInfo->sign:%s",authInfo->sign);

    //如果签名不一致则证书验证失败
    if(0!=strncmp(sign,authInfo->sign,strlen(sign)))
    {
        WRITE_IOT_ERROR_LOG("verify certificate fail!");
        return FAIL_RETURN;
    }

    return SUCCESS_RETURN;
}

int set_usr_info(AUTH_INFO_S * authInfo)
{
	ALIYUN_IOT_USER_INFO_S *userInfo = &g_userInfo;
	INT8 usrNameTmp[USER_NAME_LEN];
	//生成host address
    INT32 len = 0;
	INT8* position = NULL;
	INT8 portString[8];
	INT8* portStart = NULL;
    memset(&g_userInfo,0x0,sizeof(ALIYUN_IOT_USER_INFO_S));
    

    //生成client ID
    aliyun_iot_stdio_snprintf(userInfo->clientId, CLIENT_ID_LEN, "%s:%s", g_deviceInfo.productKey, authInfo->deviceId);
    WRITE_IOT_DEBUG_LOG("userInfo-clientId:%s",userInfo->clientId);

    //生成username
    
    memset(usrNameTmp,0x0,USER_NAME_LEN);
    aliyun_iot_stdio_snprintf(usrNameTmp, USER_NAME_LEN, "%s%s%s%s", g_deviceInfo.productKey, g_deviceInfo.productSecret, authInfo->deviceId, g_deviceInfo.deviceSecret);

    aliyun_iot_common_md5(usrNameTmp,strlen(usrNameTmp),userInfo->userName);
    WRITE_IOT_DEBUG_LOG("userInfo-userName:%s",userInfo->userName);


    position = strchr(authInfo->servers,'|');
    if(NULL == position)
    {

        WRITE_IOT_ERROR_LOG("the format of servers is error!");
        return FAIL_RETURN;
    }

    //根据协议使用类型生成host address
    
    memset(portString,0x0,8);
	
    portStart = strchr(authInfo->servers,':');
    if(NULL == portStart)
    {
        WRITE_IOT_ERROR_LOG("the format of servers is error!");
        return FAIL_RETURN;
    }

    memset(userInfo->hostAddress,0x0,HOST_ADDRESS_LEN);
    len = portStart - authInfo->servers;
    strncpy(userInfo->hostAddress,authInfo->servers,len);

    memset(userInfo->port,0x0,HOST_PORT_LEN);
    len = position - portStart;
    strncpy(portString,(portStart+1),len-1);
    memcpy(userInfo->port, portString, strlen(portString));

    WRITE_IOT_DEBUG_LOG("portString:%s",portString);
    WRITE_IOT_DEBUG_LOG("userInfo->port:%s",userInfo->port);
    WRITE_IOT_DEBUG_LOG("userInfo-hostAddress:%s",userInfo->hostAddress);

    return SUCCESS_RETURN;
}

INT32 aliyun_iot_auth_fs(SIGN_DATA_TYPE_E signDataType,AUTH_INFO_S * authInfo)
{
    int result = 0;

    //证书文件存在则需要读取文件进行鉴权
    if(0 == aliyun_iot_file_whether_exist(authInfo->trustStorePath) && 0 == aliyun_iot_file_whether_exist(authInfo->otherInfoFilePath))
    {
        //读取证书文件和签名信息
        if(SUCCESS_RETURN != aliyun_iot_read_authInfo(authInfo))
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_read_authInfo() error!");
            return ERROR_CERT_VERIFY_FAIL;
        }

        //验证证书合法性
        if(SUCCESS_RETURN != aliyun_iot_verify_certificate(authInfo,CERT_SERVER_PARAM,signDataType))
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_verify_certificate() error!");
            return ERROR_CERT_VERIFY_FAIL;
        }

        //验证成功则获取服务器IP信息
        if(0 != (result = pull_server_param(signDataType,NETWORK_SERVER_PARAM,authInfo)))
        {
            WRITE_IOT_ERROR_LOG("run pull_server_param() error!");
            return result;
        }

        //验证网络合法性
        if(SUCCESS_RETURN != aliyun_iot_verify_certificate(authInfo,NETWORK_SERVER_PARAM,signDataType))
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_verify_certificate() error!");
            return FAIL_RETURN;
        }
    }
    else
    {
        //验证成功则获取服务器IP信息
        if(0 != (result = pull_server_param(signDataType,ALL_SERVER_PARAM,authInfo)))
        {
            WRITE_IOT_ERROR_LOG("run pull_server_param() error!");
            return result;
        }

        //验证证书合法性
        if(SUCCESS_RETURN != aliyun_iot_verify_certificate(authInfo,ALL_SERVER_PARAM,signDataType))
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_verify_certificate() error!");
            return FAIL_RETURN;
        }

        //文件持久化
        if(SUCCESS_RETURN != aliyun_iot_record_authInfo(authInfo))
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_recordcrtfile error!");
            return FAIL_RETURN;
        }
    }

    //将获取到的network信息放入usrinfo内存
    if(0 != set_usr_info(authInfo))
    {
        WRITE_IOT_ERROR_LOG("run set_usr_info error!");
        return FAIL_RETURN;
    }

    return SUCCESS_RETURN;
}


INT32 aliyun_iot_auth_nofs(SIGN_DATA_TYPE_E signDataType,AUTH_INFO_S * authInfo)
{
    SERVER_PARAM_TYPE_E pulltype;
    int result = 0;

    if( AUTH_SUCCESS == authInfo->authStateInfo.authState )
    {
        //上次证书验证成功则只获取IP
		printf("[ZZZZ] pull_server_param if\n");
        if(0 != (result = pull_server_param(signDataType,NETWORK_SERVER_PARAM,authInfo)))
        {
            WRITE_IOT_ERROR_LOG("run pull_server_param() error!");
            return result;
        }
        pulltype = NETWORK_SERVER_PARAM;
    }
    else
    {
        //证书验证失败或没有验证，则此次强制获取证书和IP
		printf("[ZZZZ] pull_server_param else\n");
        if(0!= (result = pull_server_param(signDataType,ALL_SERVER_PARAM,authInfo)))
        {
            WRITE_IOT_ERROR_LOG("run pull_server_param() error!");
			printf("[ZZZZ] pull_server_param else error\n");
            return result;
        }
        WRITE_IOT_DEBUG_LOG("pubkey:%s",authInfo->pubkey);
        pulltype = ALL_SERVER_PARAM;
		printf("[ZZZZ] pull_server_param else return\n");
    }

    //验证证书或网络参数合法性
	printf("[ZZZZ] aliyun_iot_verify_certificate\n");
    if(SUCCESS_RETURN != aliyun_iot_verify_certificate(authInfo,pulltype,signDataType))
    {
        aliyun_iot_set_auth_state(AUTH_FAILS);
        WRITE_IOT_ERROR_LOG("run aliyun_iot_verify_certificate() error!");
        return ERROR_CERT_VERIFY_FAIL;
    }
	printf("[ZZZZ] set_usr_info\n");
    //将获取到的network信息放入usrinfo内存
    if(0 != set_usr_info(authInfo))
    {
        aliyun_iot_set_auth_state(AUTH_FAILS);
        WRITE_IOT_ERROR_LOG("run set_usr_info error!");
        return FAIL_RETURN;
    }
	printf("[ZZZZ] aliyun_iot_set_auth_state\n");
    aliyun_iot_set_auth_state(AUTH_SUCCESS);

    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_auth
* 描       述: sdk用户鉴权函数
* 输入参数: SIGN_DATA_TYPE_E signDataType 签名类型
*          IOT_BOOL_VALUE_E haveFilesy 是否有文件系统
* 返 回  值: 0：成功  -1：失败
* 说       明: 鉴权得到公钥证书并生成用户信息
************************************************************/
INT32 aliyun_iot_auth(SIGN_DATA_TYPE_E signDataType,IOT_BOOL_VALUE_E haveFilesys)
{
    int result  = 0;
    int rc = SUCCESS_RETURN;
    int num = 0;

    //获得存放用户信息的内存空间
    AUTH_INFO_S * authInfo = g_authInfo;
    if(NULL == authInfo)
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_get_authinfo_address() error!");
        return FAIL_RETURN;
    }

    //保存签名算法类型
    authInfo->signDataType = signDataType;

    for(;;)
    {
        if(IOT_VALUE_TRUE != haveFilesys)
        {
            //没有文件系统的鉴权操作
			printf("[ZZZZ] aliyun_iot_auth_nofs\n");
            result = aliyun_iot_auth_nofs(signDataType,authInfo);
			printf("[ZZZZ] aliyun_iot_auth_nofs return\n");
        }
        else
        {
            //带文件系统的鉴权操作
            result = aliyun_iot_auth_fs(signDataType,authInfo);
        }

        //证书验证失败
        if(ERROR_CERT_VERIFY_FAIL == result)
        {
            WRITE_IOT_ERROR_LOG("cert verify fail,num = %d!",num);
			printf("[ZZZZ] cert verify fail\n");
            //证书文件验证失败则删除
            if(IOT_VALUE_TRUE == haveFilesys)
            {
                WRITE_IOT_NOTICE_LOG("delete cert file!");

                if(0 == aliyun_iot_file_whether_exist(g_authInfo->trustStorePath))
                {
                    (void)aliyun_iot_file_delete(g_authInfo->trustStorePath);
                }

                if(0 == aliyun_iot_file_whether_exist(g_authInfo->otherInfoFilePath))
                {
                    (void)aliyun_iot_file_delete(g_authInfo->otherInfoFilePath);
                }
            }
        }

        //证书验证尝试3次，超出则退出
        if(0 != result)
        {
            WRITE_IOT_ERROR_LOG("iot auth fail! haveFilesys = %d,errorCode = %s",haveFilesys,authInfo->errCode);
			printf("[ZZZZ] iot auth fail! retry\n");
            num++;

            //证书验证失败次数小于3次则返回重新鉴权
            if(num < 3)
            {
                //多次鉴权是解决本地保存证书有误的情况
                continue;
            }

            rc = FAIL_RETURN;
        }

        break;
    }

    return rc;
}

