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
 * 获取服务器参数类型
*******************************************/
typedef enum SERVER_PARAM_TYPE
{
    ALL_SERVER_PARAM = 0,    //全部获取
    NETWORK_SERVER_PARAM,    //获取网络参数
    CERT_SERVER_PARAM,       //获取证书参数
}SERVER_PARAM_TYPE_E;

/*******************************************
 * 用户信息数据结构
*******************************************/
typedef struct ALIYUN_IOT_USER_INFO
{
    INT8   hostAddress[HOST_ADDRESS_LEN];  //鉴权服务器地址
    INT8   clientId[CLIENT_ID_LEN];        //客户ID = productKey + deviceId
    INT8   userName[USER_NAME_LEN];        //用户名 = MD5(productKey + productSecret + deviceId + deviceSecret)
    INT8   port[HOST_PORT_LEN];            //鉴权服务器端口
}ALIYUN_IOT_USER_INFO_S;

/*******************************************
 * 鉴权状态信息
*******************************************/
typedef struct USR_AUTH_STATE_INFO
{
    USER_AUTH_STATE_E authState;                 //鉴权状态
    UINT32            authStateNum;              //鉴权次数
    INT8              certSign[SIGN_STRING_LEN]; //证书签名信息
}USR_AUTH_STATE_INFO_S;

/*******************************************
 * 鉴权内容信息
*******************************************/
typedef struct AUTH_INFO
{
    USR_AUTH_STATE_INFO_S authStateInfo;                      //鉴权状态信息
    SIGN_DATA_TYPE_E      signDataType;                       //签名算法类型
    INT32                 pkVersion;                          //版本信息
    INT8                  servers[HOST_ADDRESS_LEN];          //连接服务器IP
    INT8                  sign[SIGN_STRING_LEN];              //签名信息
    INT8                  deviceId[DEVICE_ID_LEN];            //设备ID
    INT8                  trustStorePath[TRUST_STORE_PATH];   //PEM证书文件路径
    INT8                  otherInfoFilePath[TRUST_STORE_PATH];//证书签名其它信息
    INT8                  pubkey[KEY_LEN_MAX];                //PEM公钥证书内存
    char                  errCode[ERROR_CODE_LEN_MAX];        //错误编码
}AUTH_INFO_S;

/***********************************************************
* 函数名称: aliyun_iot_set_auth_state
* 描       述: 设置鉴权模式
* 输入参数: USER_AUTH_STATE_E authState
* 输出参数: VOID
* 返 回  值: VOID
* 说       明: EVERY_TIME_AUTH    每次调用aliyun_iot_auth接口都会重新获取证书鉴权
*           FIRST_CONNECT_AUTH 只在设备第一次上线时鉴权，以后使用记录的鉴权信息
************************************************************/
void aliyun_iot_set_auth_state(USER_AUTH_STATE_E authState);

int set_usr_info(AUTH_INFO_S * authInfo);
INT32 aliyun_iot_verify_certificate(AUTH_INFO_S *authInfo,SERVER_PARAM_TYPE_E pullType,SIGN_DATA_TYPE_E signDataType);
INT32 pull_server_param(SIGN_DATA_TYPE_E signDataType,SERVER_PARAM_TYPE_E pullType,AUTH_INFO_S *info);

#endif
