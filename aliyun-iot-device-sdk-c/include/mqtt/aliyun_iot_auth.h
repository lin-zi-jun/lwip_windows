/*********************************************************************************
 * �ļ�����: aliyun_iot_auth.h
 * ��       ��:
 * ��       ��: 2016-05-30
 * ��       ��: iot��Ȩ
 * ˵       ��: ���ļ������豸��IOTsdk�ļ�Ȩ�ӿں������������
 * ��       ʷ:
 **********************************************************************************/
#ifndef ALIYUN_IOT_AUTH_H
#define ALIYUN_IOT_AUTH_H

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"

#define MQTT_SDK_VERSION  "1.0.1"

/*******************************************
 * IOTsdk������жϱ�־
*******************************************/
typedef enum IOT_BOOL_VALUE
{
    IOT_VALUE_FALSE = 0,
    IOT_VALUE_TRUE,
}IOT_BOOL_VALUE_E;

/*******************************************
 * ��Ȩ״̬����
*******************************************/
typedef enum USER_AUTH_STATE
{
    AUTH_NONE = 0,     //û�м�Ȩ
    AUTH_SUCCESS,      //��Ȩ�ɹ�
    AUTH_FAILS,        //��Ȩʧ��
}USER_AUTH_STATE_E;

/*******************************************
 * ǩ������
*******************************************/
typedef enum SIGN_DATA_TYPE
{
    HMAC_MD5_SIGN_TYPE = 0, //Hmac_MD5��Ĭ�ϣ�
    HMAC_SHA1_SIGN_TYPE,    //Hmac_SHA1
    MD5_SIGN_TYPE,          //MD5
}SIGN_DATA_TYPE_E;

/*******************************************
 * �豸��ϢӰ���������ͣ�ֻ���ڲ�������
*******************************************/
typedef struct IOT_DEVICEINFO_SHADOW
{
    INT8* hostName;           //��Ȩ������
    INT8* productKey;         //��Ʒkey
    INT8* productSecret;      //��Ʒ��Կ
    INT8* deviceName;         //�豸����
    INT8* deviceSecret;       //�豸��Կ
}IOT_DEVICEINFO_SHADOW_S;

/***********************************************************
* ��������: aliyun_iot_auth_init
* ��       ��: auth��ʼ������
* �������: VOID
* �������: VOID
* �� ��  ֵ: 0 �ɹ���-1 ʧ��
* ˵       ��: ��ʼ����־�����豸��Ϣ����Ȩ��Ϣ�ļ��ı���·��
************************************************************/
INT32 aliyun_iot_auth_init();

/***********************************************************
* ��������: aliyun_iot_auth_release
* ��       ��: auth�ͷź���
* �������: VOID
* �������: VOID
* �� ��  ֵ: 0:�ɹ� -1:ʧ��
* ˵      ��: �ͷ�authInfo�ڴ�
************************************************************/
INT32 aliyun_iot_auth_release();

/***********************************************************
* ��������: aliyun_iot_set_device_info
* ��       ��: �����豸��Ϣ
* �������: IOT_DEVICEINFO_SHADOW_S*deviceInfo
* �������: VOID
* �� ��  ֵ: 0���ɹ�  -1��ʧ��
* ˵       ��: ����aliyunע����豸��Ϣ���õ�SDK�е��豸������
************************************************************/
INT32 aliyun_iot_set_device_info(IOT_DEVICEINFO_SHADOW_S*deviceInfo);

/***********************************************************
* ��������: aliyun_iot_auth
* ��       ��: sdk�û���Ȩ����
* �������: SIGN_DATA_TYPE_E signDataType ǩ������
*          IOT_BOOL_VALUE_E haveFilesys �Ƿ����ļ�ϵͳ
* �� ��  ֵ: 0���ɹ�  -1��ʧ��
* ˵       ��: ��Ȩ�õ���Կ֤�鲢�����û���Ϣ
************************************************************/
INT32 aliyun_iot_auth(SIGN_DATA_TYPE_E signDataType,IOT_BOOL_VALUE_E haveFilesys);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif


#endif
