/*******************************************************************************
 * Copyright (c) 2014 IBM Corp.
 *
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * and the Eclipse Distribution License is available at
 *   http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    Allan Stockdill-Mander/Ian Craggs - initial API and implementation and/or initial documentation
 *******************************************************************************/
/*********************************************************************************
 * �ļ�����: aliyun_iot_mqtt_client.h
 * ��       ��:
 * ��       ��: 2016-05-30
 * ��       ��:
 * ˵       ��: iot sdk��mqttЭ����û��ӿ�
 * ��       ʷ:
 **********************************************************************************/
#ifndef ALIYUN_IOT_MQTT_CLIENT_H
#define ALIYUN_IOT_MQTT_CLIENT_H

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#include <stdio.h>

#include "aliyun_iot_mqtt_nettype.h"
#include "aliyun_iot_common_list.h"
#include "aliyun_iot_mqtt_common.h"
#include "aliyun_iot_platform_pthread.h"
#include "aliyun_iot_platform_threadsync.h"
#include "MQTTPacket.h"

typedef struct Client Client;
typedef struct SUBSCRIBE_INFO SUBSCRIBE_INFO_S;

#define MAX_PACKET_ID 65535
#define MAX_MESSAGE_HANDLERS 5
#define MQTT_TOPIC_LEN_MAX 128
#define MQTT_REPUB_NUM_MAX 20
#define MQTT_SUB_NUM_MAX   10

/*******************************************
 * MQTT�Զ���������С����ʱ�䣬��λms
*******************************************/
#define ALI_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL 1000

/*******************************************
 * MQTT�Զ��������������ʱ�䣬��λms
*******************************************/
#define ALI_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL 60000

/*******************************************
 * �豸ɾ����MQTT�Զ�����������ʱ�䣬��λms
*******************************************/
#define ALI_IOT_MQTT_DEVICE_ABNORMAL_RECONNECT_WAIT_INTERVAL 0xFFFFFFFF

/*******************************************
 * MQTT��Ϣ������С��ʱʱ�䣬��λms
*******************************************/
#define ALI_IOT_MQTT_MIN_COMMAND_TIMEOUT    500

/*******************************************
 * MQTT��Ϣ�������ʱʱ�䣬��λms
*******************************************/
#define ALI_IOT_MQTT_MAX_COMMAND_TIMEOUT    5000

/*******************************************
 * MQTT��Ϣ�������ʱʱ�䣬��λms
*******************************************/
#define ALI_IOT_MQTT_DEFAULT_COMMAND_TIMEOUT 2000

/*******************************************
 * mqtt��Ϣ��QOS������
*******************************************/
enum QoS { QOS0, QOS1, QOS2 };

/*******************************************
 * �ڵ�״̬���
*******************************************/
typedef enum MQTT_NODE_STATE
{
    NODE_NORMANL_STATE = 0,
    NODE_INVALID_STATE,
}MQTT_NODE_STATE_E;

/*******************************************
 * mqtt��Ϣ�ṹ��
*******************************************/
typedef struct MQTTMessage
{
    enum QoS qos;          //QOS���ͣ���mqtt����ȼ�����ǰ֧��QOS0��QOS1
    char retained;         //publish���ı�����־
    char dup;              //���Ʊ����ظ��ַ���־
    unsigned short id;     //���ı�ʶ��
    void *payload;         //���ĸ�������,ע�⣺�������ݿ���ʹ�ö��������ݻ��ַ��ı�����
    size_t payloadlen;     //���ĸ������ݳ���,ע�⣺ʹ���ַ��ı�����ʱpayloadlen���ַ������Ȳ�����������'\0'��־�����������ݵĳ�����Ҫ�û�׼ȷָ��.
}MQTTMessage;


/*******************************************
 * ��Ϣ����ص��������ݽṹ
*******************************************/
typedef struct MessageData
{
    MQTTMessage* message;   //mqtt��Ϣ�ṹ��
    MQTTString* topicName;  //���ĵ�����topic
}MessageData;

/*******************************************
 * mqtt��Ϣ����ص�����
*******************************************/
typedef void (*messageHandler)(MessageData*);

/*******************************************
 * mqtt client״̬����
*******************************************/
typedef enum Mystate
{
	CLIENT_STATE_INVALID = 0,                    //client��Ч״̬
	CLIENT_STATE_INITIALIZED = 1,                //client��ʼ��״̬
	CLIENT_STATE_CONNECTED = 2,                  //client�Ѿ�����״̬
	CLIENT_STATE_DISCONNECTED = 3,               //client���Ӷ�ʧ״̬
	CLIENT_STATE_DISCONNECTED_RECONNECTING = 4,  //client��������״̬
}MQTTClientState;

/*******************************************
 * �����¼����ͣ���������ײ��¼��ص���
*******************************************/
typedef enum
{
    ALI_IOT_NETWORK_CONNECTED = 0,          //��������
    ALI_IOT_NETWORK_DISCONNECTED = 1,       //����ʧȥ����
    ALI_IOT_NETWORK_LINK_LOSS = 2,		    //�������Ӷ�ʧ
    ALI_IOT_NETWORK_MAX_NUMBER              //�������������
} ALIYUN_IOT_NETWORK_E;

/*******************************************
 * �������Ӷ�ʧ�ص�������
*******************************************/
typedef void (*iot_disconnect_handler)(Client *, void *);

/*******************************************
 * �������Ӷ�ʧ�ص�������
*******************************************/
typedef void (*iot_reconnect_handler)(Client *, void *);

/*******************************************
 * ��Ϣ������ɻص��������յ���ϢACK��
*******************************************/
typedef void DeliveryComplete(void* context, unsigned int msgId);

/*******************************************
 * ������Ϣ��ʱ�ص�������û���յ���ϢACK��
*******************************************/
typedef void SubAckTimeoutHandler(SUBSCRIBE_INFO_S *);

/*******************************************
 * mqtt client�ĳ�ʼ������
*******************************************/
typedef struct
{
	unsigned int           mqttCommandTimeout_ms;	//MQTT��Ϣ���䳬ʱʱ�䣬ʱ�����������䡣��λ������
	iot_disconnect_handler disconnectHandler;	    //�������Ӷ�ʧ�ص�����������ʹ����NULL
	iot_reconnect_handler  reconnectHandler;        //�������ӻָ��ص�����������ʹ����NULL
	void                   *disconnectHandlerData;	//�������Ӷ�ʧ�ص������������
	void                   *reconnectHandlerData;   //�������ӻָ��ص������������
	unsigned char          *pWriteBuf;		        //MQTT����buffer
	unsigned int           writeBufSize;            //MQTT����buffer�ĳ���,
	unsigned char          *pReadBuf; 	            //MQTT����buffer
	unsigned int           readBufSize;             //MQTT����buffer�ĳ���
	DeliveryComplete       *deliveryCompleteFun;    //MQTT������ɻص���������ʹ����NULL
	SubAckTimeoutHandler   *subAckTimeOutFun;       //MQTTSub��unSub��ϢACK��ʱ�Ļص�
} IOT_CLIENT_INIT_PARAMS;

/*******************************************
 * mqtt client������������
*******************************************/
typedef struct
{
	Timer                  reconnectDelayTimer;	            //������ʱ�����ж��Ƿ�����ʱ��
	iot_disconnect_handler disconnectHandler;               //�������Ӷ�ʧ�ص�������
	iot_reconnect_handler  reconnectHandler;                //�������ӻָ��ص�������
	void                   *disconnectHandlerData;	        //�������Ӷ�ʧ�ص������������
	void                   *reconnectHandlerData;           //�������ӻָ��ص������������
	BOOL                   isAutoReconnectEnabled;	        //�Զ�������־
	unsigned int           currentReconnectWaitInterval;	//��������ʱ�����ڣ���λms
	int                    reconnectNum;                    //��������ʧ�ܴ���
}IOT_CLIENT_RECONNECT_PARAMS;

/*******************************************
 * ���������Ӧ����Ϣ����ṹ
*******************************************/
typedef struct MessageHandlers
{
    const char* topicFilter;      //pub��Ϣ��Ӧ������
    void (*fp) (MessageData*);    //pub��Ϣ������
}MessageHandlers;

/*******************************************
 *         sub��Ϣ����Ϣ��¼
*******************************************/
struct SUBSCRIBE_INFO
{
    enum msgTypes    type;                               //sub��Ϣ���ͣ�sub or unsub��
    unsigned int     msgId;                              //sub���ı�ʶ��
    Timer            subTime;                            //sub��Ϣʱ��
    MQTT_NODE_STATE_E nodeState;                         //node״̬
    MessageHandlers  handler;
    int              len;                                //sub��Ϣ����
    unsigned char*   buf;                                //sub��Ϣ��
};

/*******************************************
 * ����ָ��ź�
*******************************************/
typedef struct NETWORK_RECOVER_CALLBACK_SIGNAL
{
    int                   signal;      //�źű�־
    ALIYUN_IOT_MUTEX_S    signalLock;  //�ź���
}NETWORK_RECOVER_CALLBACK_SIGNAL_S;

/*******************************************
 * MQTT CLIENT���ݽṹ
*******************************************/
struct Client
{
    unsigned int                      next_packetid;                           //MQTT���ı�ʶ��
    ALIYUN_IOT_MUTEX_S                idLock;                                  //MQTT���ı�־����
    unsigned int                      command_timeout_ms;                      //MQTT��Ϣ���䳬ʱʱ�䣬ʱ�����������䡣��λ������
    int                               threadRunning;                           //client���߳����б�־
    size_t                            buf_size, readbuf_size;                  //MQTT��Ϣ���ͽ���buffer�Ĵ�С
    unsigned char                     *buf;                                    //MQTT��Ϣ����buffer
    unsigned char                     *readbuf;                                //MQTT��Ϣ����buffer
    MessageHandlers                   messageHandlers[MAX_MESSAGE_HANDLERS];   //���������Ӧ����Ϣ����ṹ����
    void (*defaultMessageHandler)     (MessageData*);                          //����Ĭ����Ϣ����ṹ
    DeliveryComplete                  *deliveryCompleteFun;                    //MQTT��Ϣ������ɻص��������յ���ϢACK��
    Network*                          ipstack;                                 //MQTTʹ�õ��������
    Timer                             ping_timer;                              //MQTT���ʱ����ʱ��δ���������������
    int                               pingMark;                                //ping��Ϣ���ͱ�־
    ALIYUN_IOT_MUTEX_S                pingMarkLock;                            //ping��Ϣ���ͱ�־��
    MQTTClientState                   clientState;                             //MQTT client״̬
    ALIYUN_IOT_MUTEX_S                stateLock;                               //MQTT client״̬��
    IOT_CLIENT_RECONNECT_PARAMS       reconnectparams;                         //MQTT client��������
	MQTTPacket_connectData            connectdata;                             //MQTT������������
	list_t *                          pubInfoList;                             //publish��Ϣ��Ϣ����
	list_t *                          subInfoList;                             //subscribe��unsubscribe��Ϣ��Ϣ����
	ALIYUN_IOT_MUTEX_S                pubInfoLock;                             //publish��Ϣ��Ϣ������
	ALIYUN_IOT_MUTEX_S                subInfoLock;                             //subscribe��Ϣ��Ϣ������
	ALIYUN_IOT_PTHREAD_S              recieveThread;                           //MQTT������Ϣ�߳�
	ALIYUN_IOT_PTHREAD_S              keepaliveThread;                         //MQTT client�����߳�
	ALIYUN_IOT_PTHREAD_S              retransThread;                           //MQTT pub��Ϣ�ط��߳�
	ALIYUN_IOT_MUTEX_S                writebufLock;                            //MQTT��Ϣ����buffer��
	NETWORK_RECOVER_CALLBACK_SIGNAL_S networkRecoverSignal;                    //����ָ��ź�
	ALIYUN_IOT_SEM_S                  semaphore;                               //������Ϣͬ���ź�
	SubAckTimeoutHandler              *subAckTimeOutFun;                       //sub��unsub��Ϣack��ʱ�ص�
};

/***********************************************************
* ��������: aliyun_iot_mqtt_init
* ��       ��: mqtt��ʼ��
* �������: Client *pClient
*          IOT_CLIENT_INIT_PARAMS *pInitParams
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: �û�����Client�ṹ���˽ӿڸ����û��ĳ�ʼ��������ʼ��Client�ṹ
************************************************************/
int aliyun_iot_mqtt_init(Client *pClient, IOT_CLIENT_INIT_PARAMS *pInitParams) ;

/***********************************************************
* ��������: aliyun_iot_mqtt_release
* ��       ��: mqtt�ͷ�
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: �ͷ�mqtt��ʼ��ʱ��������Դ
************************************************************/
int aliyun_iot_mqtt_release(Client *pClient);

/***********************************************************
* ��������: aliyun_iot_mqtt_connect
* ��       ��: ����mqttЭ��
* �������: Client *pClient
*           MQTTPacket_connectData* pConnectParams ���ӱ��Ĳ���
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ʵ������connect��mqttЭ���connect
*          �˽ӿ���ͬ��CONNACK�������ӿڣ���ʱ�˳�
************************************************************/
int aliyun_iot_mqtt_connect(Client* pClient, MQTTPacket_connectData* pConnectParams);

/***********************************************************
* ��������: aliyun_iot_mqtt_disconnect
* ��       ��: �Ͽ�mqttЭ������
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: mqttЭ���disconnect�������disconnect
*          ע��ִ�н׶ε��ô˽ӿڻᴥ���Զ�����
************************************************************/
int aliyun_iot_mqtt_disconnect(Client* pClient);

/***********************************************************
* ��������: aliyun_iot_mqtt_subscribe
* ��       ��: mqtt������Ϣ
* �������: Client* c  mqtt�ͻ���
*          char* topicFilter ���ĵ��������
*          enum QoS qos ��Ϣ��������
*          messageHandler messageHandler ��Ϣ����ṹ
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: MQTT���Ĳ��������ĳɹ������յ�sub ack
************************************************************/
int aliyun_iot_mqtt_subscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler);

/***********************************************************
* ��������: aliyun_iot_mqtt_unsubscribe
* ��       ��: mqttȡ������
* �������: Client* c  mqtt�ͻ���
*          char* topicFilter ���ĵ��������
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ͬsubscribe
************************************************************/
int aliyun_iot_mqtt_unsubscribe(Client* c, const char* topicFilter);

/***********************************************************
* ��������: aliyun_iot_mqtt_sub_sync
* ��       ��: mqttͬ������ACK
* �������: Client* c  mqtt�ͻ���
*          char* topicFilter ���ĵ��������
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ͬsubscribe
************************************************************/
int aliyun_iot_mqtt_suback_sync(Client* c,const char* topicFilter,messageHandler messageHandler);

/***********************************************************
* ��������: aliyun_iot_mqtt_publish
* ��       ��: mqtt������Ϣ
* �������: Client* c  mqtt�ͻ���
*          char* topicName ������Ϣ������
*          MQTTMessage* message ��������Ϣ��
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: MQTT������Ϣ�����������ɹ������յ�pub ack
************************************************************/
int aliyun_iot_mqtt_publish(Client* c, const char* topicName, MQTTMessage* message);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif

#endif
