#ifndef ALIYUN_IOT_MQTT_INTERNAL_H
#define ALIYUN_IOT_MQTT_INTERNAL_H

#define HOST_ADDRESS_LEN   64
#define CLIENT_ID_LEN      128
#define USER_NAME_LEN      256
#define HOST_PORT_LEN      8
#define SIGN_STRING_LEN    64
#define DEVICE_ID_LEN      64
#define TRUST_STORE_PATH   64
#define KEY_LEN_MAX        2560
#define ERROR_CODE_LEN_MAX 16

#define STRING_RESPOSE    3072
#define STRING_IP_RESPOSE 1024
#define STRING_LEN        1024
#define SIGN_METHOD_LEN   16
#define SIGN_KEY_LEN      128

#define PINGRSP_TIMEOUT_MS           5000
#define CHANGE_IP_RECONNECT_NUM_MAX  2

/*******************************************
 * ��ȡ��������������
*******************************************/
typedef enum SERVER_PARAM_TYPE
{
    ALL_SERVER_PARAM = 0,    //ȫ����ȡ
    NETWORK_SERVER_PARAM,    //��ȡ�������
    CERT_SERVER_PARAM,       //��ȡ֤�����
}SERVER_PARAM_TYPE_E;

/*******************************************
 * �û���Ϣ���ݽṹ
*******************************************/
typedef struct ALIYUN_IOT_USER_INFO
{
    INT8   hostAddress[HOST_ADDRESS_LEN];  //��Ȩ��������ַ
    INT8   clientId[CLIENT_ID_LEN];        //�ͻ�ID = productKey + deviceId
    INT8   userName[USER_NAME_LEN];        //�û��� = MD5(productKey + productSecret + deviceId + deviceSecret)
    INT8   port[HOST_PORT_LEN];            //��Ȩ�������˿�
}ALIYUN_IOT_USER_INFO_S;

/*******************************************
 * ��Ȩ״̬��Ϣ
*******************************************/
typedef struct USR_AUTH_STATE_INFO
{
    USER_AUTH_STATE_E authState;                 //��Ȩ״̬
    UINT32            authStateNum;              //��Ȩ����
    INT8              certSign[SIGN_STRING_LEN]; //֤��ǩ����Ϣ
}USR_AUTH_STATE_INFO_S;

/*******************************************
 * ��Ȩ������Ϣ
*******************************************/
typedef struct AUTH_INFO
{
    USR_AUTH_STATE_INFO_S authStateInfo;                      //��Ȩ״̬��Ϣ
    SIGN_DATA_TYPE_E      signDataType;                       //ǩ���㷨����
    INT32                 pkVersion;                          //�汾��Ϣ
    INT8                  servers[HOST_ADDRESS_LEN];          //���ӷ�����IP
    INT8                  sign[SIGN_STRING_LEN];              //ǩ����Ϣ
    INT8                  deviceId[DEVICE_ID_LEN];            //�豸ID
    INT8                  trustStorePath[TRUST_STORE_PATH];   //PEM֤���ļ�·��
    INT8                  otherInfoFilePath[TRUST_STORE_PATH];//֤��ǩ��������Ϣ
    INT8                  pubkey[KEY_LEN_MAX];                //PEM��Կ֤���ڴ�
    char                  errCode[ERROR_CODE_LEN_MAX];        //�������
}AUTH_INFO_S;

/***********************************************************
* ��������: aliyun_iot_set_auth_state
* ��       ��: ���ü�Ȩģʽ
* �������: USER_AUTH_STATE_E authState
* �������: VOID
* �� ��  ֵ: VOID
* ˵       ��: EVERY_TIME_AUTH    ÿ�ε���aliyun_iot_auth�ӿڶ������»�ȡ֤���Ȩ
*           FIRST_CONNECT_AUTH ֻ���豸��һ������ʱ��Ȩ���Ժ�ʹ�ü�¼�ļ�Ȩ��Ϣ
************************************************************/
void aliyun_iot_set_auth_state(USER_AUTH_STATE_E authState);

int set_usr_info(AUTH_INFO_S * authInfo);
INT32 aliyun_iot_verify_certificate(AUTH_INFO_S *authInfo,SERVER_PARAM_TYPE_E pullType,SIGN_DATA_TYPE_E signDataType);
INT32 pull_server_param(SIGN_DATA_TYPE_E signDataType,SERVER_PARAM_TYPE_E pullType,AUTH_INFO_S *info);

#endif
