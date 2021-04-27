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

#include "aliyun_iot_mqtt_client.h"
#include "aliyun_iot_platform_pthread.h"
#include "aliyun_iot_platform_memory.h"
#include "aliyun_iot_common_error.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_auth.h"
#include "aliyun_iot_mqtt_internal.h"
#include "aliyun_iot_common_util.h"

typedef enum
{
    CONNECTION_ACCEPTED = 0,
    CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION = 1,
    CONNECTION_REFUSED_IDENTIFIER_REJECTED = 2,
    CONNECTION_REFUSED_SERVER_UNAVAILABLE = 3,
    CONNECTION_REFUSED_BAD_USERDATA = 4,
    CONNECTION_REFUSED_NOT_AUTHORIZED = 5
} MQTT_ConnectAck_Return_Codes;

typedef struct REPUBLISH_INFO
{
    Timer             pubTime;      //pub��Ϣ��ʱ��
    MQTT_NODE_STATE_E nodeState;    //node״̬
    unsigned int      msgId;        //pub��Ϣ�ı��ı�ʶ��
    int               len;          //pub��Ϣ����
    unsigned char*    buf;          //pub��Ϣ��
}REPUBLISH_INFO_S;

int sendPacket(Client* c,unsigned char *buf,int length, Timer* timer);
MQTTClientState aliyun_iot_mqtt_get_client_state(Client *pClient);
void aliyun_iot_mqtt_set_client_state(Client *pClient, MQTTClientState newState);
void aliyun_iot_mqtt_yield(Client* pClient, int timeout_ms);
int aliyun_iot_mqtt_keep_alive(Client *pClient);
void aliyun_iot_mqtt_disconnect_callback(Client *pClient) ;
BOOL whether_mqtt_client_state_normal(Client*c);
int aliyun_iot_mqtt_handle_reconnect(Client *pClient);
void aliyun_iot_mqtt_reconnect_callback(Client *pClient);
int push_pubInfo_to(Client* c,int len,unsigned short msgId,list_node_t** node);
int push_subInfo_to(Client* c,int len,unsigned short msgId,enum msgTypes type,MessageHandlers *handler,list_node_t** node);
static int whether_messagehandler_same(MessageHandlers *messageHandlers1,MessageHandlers *messageHandler2);
extern ALIYUN_IOT_USER_INFO_S g_userInfo;
extern AUTH_INFO_S *g_authInfo;

Client *pClientStatus = NULL;

/***********************************************************
* ��������: MQTTKeepalive
* ��       ��: mqtt���������ͽӿ�
* �������: Client*pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int MQTTKeepalive(Client*pClient)
{
    /* there is no ping outstanding - send ping packet */
    Timer timer;
	int len = 0;
    int rc = 0;
    InitTimer(&timer);
    countdown_ms(&timer, 1000);


    aliyun_iot_mutex_lock(&pClient->writebufLock);
    len = MQTTSerialize_pingreq(pClient->buf, pClient->buf_size);
    if(len <= 0)
    {
        aliyun_iot_mutex_unlock(&pClient->writebufLock);
        WRITE_IOT_ERROR_LOG("Serialize ping request is error");
        return MQTT_PING_PACKET_ERROR;
    }

    rc = sendPacket(pClient, pClient->buf,len, &timer);
    if(SUCCESS_RETURN != rc)
    {
        aliyun_iot_mutex_unlock(&pClient->writebufLock);
        /*ping outstanding , then close socket  unsubcribe topic and handle callback function*/
        WRITE_IOT_ERROR_LOG("ping outstanding is error,result = %d",rc);
        return MQTT_NETWORK_ERROR;
    }
    aliyun_iot_mutex_unlock(&pClient->writebufLock);

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: MQTTConnect
* ��       ��: mqtt���Ӵ�����ͽӿ�
* �������: Client*pClient
*           MQTTPacket_connectData* pConnectParams
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int MQTTConnect(Client*pClient)
{
    MQTTPacket_connectData* pConnectParams = &pClient->connectdata;
    Timer connectTimer;
    int len = 0;
    int rc = 0;

    aliyun_iot_mutex_lock(&pClient->writebufLock);
    if ((len = MQTTSerialize_connect(pClient->buf, pClient->buf_size, pConnectParams)) <= 0)
    {
        aliyun_iot_mutex_unlock(&pClient->writebufLock);
        WRITE_IOT_ERROR_LOG("Serialize connect packet failed,len = %d",len);
        return MQTT_CONNECT_PACKET_ERROR;
    }

    /* send the connect packet*/
    InitTimer(&connectTimer);
    countdown_ms(&connectTimer, pClient->command_timeout_ms);
    if ((rc = sendPacket(pClient, pClient->buf,len, &connectTimer)) != SUCCESS_RETURN)
    {
        aliyun_iot_mutex_unlock(&pClient->writebufLock);
        WRITE_IOT_ERROR_LOG("send connect packet failed");
        return MQTT_NETWORK_ERROR;
    }
    aliyun_iot_mutex_unlock(&pClient->writebufLock);

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: MQTTPublish
* ��       ��: mqtt������Ϣ������ͽӿ�
* �������: Client*pClient
*           const char* topicName
*           MQTTMessage* message
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int MQTTPublish(Client*c,const char* topicName, MQTTMessage* message)
{
    Timer timer;
    INT32 lefttime = 0;
    MQTTString topic = MQTTString_initializer;
    
    int len = 0;
	list_node_t *node = NULL;
	topic.cstring = (char *)topicName;

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    aliyun_iot_mutex_lock(&c->writebufLock);
    len = MQTTSerialize_publish(c->buf, c->buf_size, 0, message->qos, message->retained, message->id, topic, (unsigned char*)message->payload, message->payloadlen);
    if (len <= 0)
    {
        aliyun_iot_mutex_unlock(&c->writebufLock);
        WRITE_IOT_ERROR_LOG("MQTTSerialize_publish is error,rc = %d, buf_size = %ld, payloadlen = %ld",len,c->buf_size,message->payloadlen);
        return MQTT_PUBLISH_PACKET_ERROR;
    }

    if(message->qos > QOS0)
    {
        //��pub��Ϣ���浽pubInfoList��
        if (SUCCESS_RETURN != push_pubInfo_to(c,len,message->id,&node))
        {
            WRITE_IOT_ERROR_LOG("push publish into to pubInfolist failed!");
            aliyun_iot_mutex_unlock(&c->writebufLock);
            return MQTT_PUSH_TO_LIST_ERROR;
        }
    }

    if (sendPacket(c, c->buf,len, &timer) != SUCCESS_RETURN) // send the subscribe packet
    {
        if(message->qos > QOS0)
        {
            //����ʧ����ɾ��֮ǰ����pubInfoList�����еĽڵ�
            aliyun_iot_mutex_lock(&c->pubInfoLock);
            list_remove(c->pubInfoList, node);
            aliyun_iot_mutex_unlock(&c->pubInfoLock);
        }

        lefttime = left_ms(&timer);
        WRITE_IOT_ERROR_LOG("sendPacket failed,lefttime = %d!",lefttime);
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_NETWORK_ERROR;
    }

    aliyun_iot_mutex_unlock(&c->writebufLock);
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: MQTTPuback
* ��       ��: mqtt������ϢACK������ͽӿ�
* �������: Client*pClient
*           unsigned int msgId
*           enum msgTypes type
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int MQTTPuback(Client*c,unsigned int msgId,enum msgTypes type)
{
    int rc = 0;
    int len = 0;
    Timer timer;
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    aliyun_iot_mutex_lock(&c->writebufLock);
    if (type == PUBACK)
    {
        len = MQTTSerialize_ack(c->buf, c->buf_size, PUBACK, 0, msgId);
    }
    else if (type == PUBREC)
    {
        len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREC, 0, msgId);
    }
    else if(type == PUBREL)
    {
        len = MQTTSerialize_ack(c->buf, c->buf_size, PUBREL, 0, msgId);
    }
    else
    {
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_PUBLISH_ACK_TYPE_ERROR;
    }

    if (len <= 0)
    {
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_PUBLISH_ACK_PACKET_ERROR;
    }

    rc = sendPacket(c, c->buf,len, &timer);
    if (rc != SUCCESS_RETURN)
    {
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_NETWORK_ERROR;
    }

    aliyun_iot_mutex_unlock(&c->writebufLock);
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: MQTTSubscribe
* ��       ��: mqtt������Ϣ������ͽӿ�
* �������: Client*pClient
*           char* topicFilter
*           enum QoS qos
*           unsigned int msgId
*           messageHandler messageHandler
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int MQTTSubscribe(Client *c,const char* topicFilter, enum QoS qos,unsigned int msgId,messageHandler messageHandler)
{
    int rc = 0;
    int len = 0;
    MQTTString topic = MQTTString_initializer;
	Timer timer;
	//��sub��Ϣ���浽subInfoList��
    list_node_t *node = NULL;
	MessageHandlers handler;
    topic.cstring = (char *)topicFilter;

    
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    aliyun_iot_mutex_lock(&c->writebufLock);

    len = MQTTSerialize_subscribe(c->buf, c->buf_size, 0, (unsigned short)msgId, 1, &topic, (int*)&qos);
    if (len <= 0)
    {
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_SUBSCRIBE_PACKET_ERROR;
    }

    //handler = {topicFilter,messageHandler};
	handler.topicFilter = topicFilter;
	handler.fp = messageHandler;
    
    if (SUCCESS_RETURN != push_subInfo_to(c,len,msgId,SUBSCRIBE,&handler,&node))
    {
        WRITE_IOT_ERROR_LOG("push publish into to pubInfolist failed!");
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_PUSH_TO_LIST_ERROR;
    }

    if ((rc = sendPacket(c, c->buf,len, &timer)) != SUCCESS_RETURN) // send the subscribe packet
    {
        //����ʧ����ɾ��֮ǰ����subInfoList�����еĽڵ�
        aliyun_iot_mutex_lock(&c->subInfoLock);
        list_remove(c->subInfoList, node);
        aliyun_iot_mutex_unlock(&c->subInfoLock);
        aliyun_iot_mutex_unlock(&c->writebufLock);
        WRITE_IOT_ERROR_LOG("run sendPacket error!");
        return MQTT_NETWORK_ERROR;
    }

    aliyun_iot_mutex_unlock(&c->writebufLock);
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: MQTTUnsubscribe
* ��       ��: mqttȡ��������Ϣ������ͽӿ�
* �������: Client*pClient
*           const char* topicFilter
*           unsigned int msgId
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int MQTTUnsubscribe(Client *c,const char* topicFilter, unsigned int msgId)
{
    Timer timer;
    MQTTString topic = MQTTString_initializer;
    
    int len = 0;
    int rc = 0;
	//��sub��Ϣ���浽subInfoList��
    list_node_t *node = NULL;
	MessageHandlers handler;
	topic.cstring = (char *)topicFilter;
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    aliyun_iot_mutex_lock(&c->writebufLock);

    if ((len = MQTTSerialize_unsubscribe(c->buf, c->buf_size, 0, (unsigned short)msgId, 1, &topic)) <= 0)
    {
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_UNSUBSCRIBE_PACKET_ERROR;
    }

    //MessageHandlers handler = {topicFilter,NULL};
	handler.fp = NULL;
    handler.topicFilter = topicFilter;
    if (SUCCESS_RETURN != push_subInfo_to(c,len,msgId,UNSUBSCRIBE,&handler,&node))
    {
        WRITE_IOT_ERROR_LOG("push publish into to pubInfolist failed!");
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_PUSH_TO_LIST_ERROR;
    }

    if ((rc = sendPacket(c, c->buf,len, &timer)) != SUCCESS_RETURN) // send the subscribe packet
    {
        //����ʧ����ɾ��֮ǰ����subInfoList�����еĽڵ�
        aliyun_iot_mutex_lock(&c->subInfoLock);
        list_remove(c->subInfoList, node);
        aliyun_iot_mutex_unlock(&c->subInfoLock);
        aliyun_iot_mutex_unlock(&c->writebufLock);
        return MQTT_NETWORK_ERROR;
    }

    aliyun_iot_mutex_unlock(&c->writebufLock);

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: MQTTDisconnect
* ��       ��: mqtt�Ͽ�������Ϣ������ͽӿ�
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: mqttЭ���Ͽ�����
************************************************************/
int MQTTDisconnect(Client* c)
{
    int rc = FAIL_RETURN;
    Timer timer;     // we might wait for incomplete incoming publishes to complete
	int len = 0;
    aliyun_iot_mutex_lock(&c->writebufLock);
    len = MQTTSerialize_disconnect(c->buf, c->buf_size);

    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (len > 0)
    {
        rc = sendPacket(c, c->buf,len, &timer);            // send the disconnect packet
    }

    aliyun_iot_mutex_unlock(&c->writebufLock);

    return rc;
}

/***********************************************************
* ��������: sendNetworkRecoverSignal
* ��       ��: ��������ָ��ź�
* �������: NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: mqttЭ���Ͽ�����
************************************************************/
void sendNetworkRecoverSignal(NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal)
{
    aliyun_iot_mutex_lock(&networkRecoverSignal->signalLock);
    networkRecoverSignal->signal = 1;
    aliyun_iot_mutex_unlock(&networkRecoverSignal->signalLock);
}

/***********************************************************
* ��������: waitNetworkRecoverSignal
* ��       ��: �ȴ�����ָ��ź�
* �������: NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: �������ȴ����ж��ź��Ƿ���ֱ���˳�
************************************************************/
int waitNetworkRecoverSignal(NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal)
{
    int signal = 0;
    aliyun_iot_mutex_lock(&networkRecoverSignal->signalLock);
    signal = networkRecoverSignal->signal;
    aliyun_iot_mutex_unlock(&networkRecoverSignal->signalLock);

    if(signal == 0)
    {
        return FAIL_RETURN;
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: networkRecoverSignalInit
* ��       ��: ����ָ��źų�ʼ��
* �������: NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
void networkRecoverSignalInit(NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal)
{
    networkRecoverSignal->signal = 0;
    aliyun_iot_mutex_init(&networkRecoverSignal->signalLock);
}

/***********************************************************
* ��������: networkRecoverSignalRelease
* ��       ��: ����ָ��ź��ͷ�
* �������: NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
void networkRecoverSignalRelease(NETWORK_RECOVER_CALLBACK_SIGNAL_S *networkRecoverSignal)
{
    networkRecoverSignal->signal = 0;
    aliyun_iot_mutex_destory(&networkRecoverSignal->signalLock);
}

/***********************************************************
* ��������: mask_pubInfo_from
* ��       ��: ��Ǵ�ɾ����pubinfo��Ϣ
* �������: Client* c
*          unsigned int  msgId
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ɾ����ӦmsgId��pubinfo��Ϣ���ڽ��յ�puback����ô˽ӿ�ɾ����Ӧpub��Ϣ
************************************************************/
int mask_pubInfo_from(Client* c,unsigned int  msgId)
{
	list_iterator_t *iter = NULL;
	list_node_t *node = NULL;
    REPUBLISH_INFO_S *repubInfo = NULL;
    aliyun_iot_mutex_lock(&c->pubInfoLock);
    if (c->pubInfoList->len)
    {
        iter = list_iterator_new(c->pubInfoList, LIST_TAIL);
        for (;;)
        {
            node = list_iterator_next(iter);

            if (NULL == node)
            {
                break;
            }

            repubInfo = (REPUBLISH_INFO_S *) node->val;
            if (NULL == repubInfo)
            {
                WRITE_IOT_ERROR_LOG("node's value is invalid!");
                continue;
            }

            if (repubInfo->msgId == msgId)
            {
                //�����Ч�ڵ�
                repubInfo->nodeState = NODE_INVALID_STATE;
            }
        }

        list_iterator_destroy(iter);
    }
    aliyun_iot_mutex_unlock(&c->pubInfoLock);

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: push_pubInfo_to
* ��       ��: ��pubinfo��Ϣ����pubInfoList����
* �������: Client* c
*          PUBLISH_STRUCT_INFO_S *pubInfo
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��: ��pubinfo��Ϣ���룬�ڷ���pub��Ϣʱ����
************************************************************/
int push_pubInfo_to(Client* c,int len,unsigned short msgId,list_node_t** node)
{
	//�����ڴ�ռ�
    REPUBLISH_INFO_S *repubInfo = NULL;
    if(len < 0 || len > c->buf_size)
    {
        WRITE_IOT_ERROR_LOG("the param of len is error!")
        return FAIL_RETURN;
    }

    aliyun_iot_mutex_lock(&c->pubInfoLock);

    if(c->pubInfoList->len >= MQTT_REPUB_NUM_MAX)
    {
        aliyun_iot_mutex_unlock(&c->pubInfoLock);
        WRITE_IOT_ERROR_LOG("number of repubInfo more than max!,size = %d",c->pubInfoList->len)
        return FAIL_RETURN;
    }

    //�����ڴ�ռ�
    repubInfo = (REPUBLISH_INFO_S*)aliyun_iot_memory_malloc(sizeof(REPUBLISH_INFO_S)+len);
    if(NULL == repubInfo)
    {
        aliyun_iot_mutex_unlock(&c->pubInfoLock);
        WRITE_IOT_ERROR_LOG("run aliyun_iot_memory_malloc is error!");
        return FAIL_RETURN;
    }

    repubInfo->nodeState = NODE_NORMANL_STATE;
    repubInfo->msgId = msgId;
    repubInfo->len = len;
    StartTimer(&repubInfo->pubTime);
    repubInfo->buf = (unsigned char*)repubInfo + sizeof(REPUBLISH_INFO_S);

    //���Ʊ��������
    memcpy(repubInfo->buf,c->buf,len);

    //����������
    *node = list_node_new(repubInfo);
    if(NULL == *node)
    {
        aliyun_iot_mutex_unlock(&c->pubInfoLock);
        WRITE_IOT_ERROR_LOG("run list_node_new is error!");
        return FAIL_RETURN;
    }

    //��������������
    list_rpush(c->pubInfoList,*node);

    aliyun_iot_mutex_unlock(&c->pubInfoLock);

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: push_subInfo_to
* ��       ��: ���subinfo��Ϣ
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��: ��subinfo��Ϣ���룬�ڷ���sub��Ϣʱ����
************************************************************/
int push_subInfo_to(Client* c,int len,unsigned short msgId,enum msgTypes type,MessageHandlers *handler,list_node_t** node)
{
	//�����ڴ�ռ�
    SUBSCRIBE_INFO_S *subInfo = NULL;
    aliyun_iot_mutex_lock(&c->subInfoLock);

    if(c->subInfoList->len >= MQTT_SUB_NUM_MAX)
    {
        aliyun_iot_mutex_unlock(&c->subInfoLock);
        WRITE_IOT_ERROR_LOG("number of subInfo more than max!,size = %d",c->subInfoList->len)
        return FAIL_RETURN;
    }


    //�����ڴ�ռ�
    subInfo = (SUBSCRIBE_INFO_S*)aliyun_iot_memory_malloc(sizeof(SUBSCRIBE_INFO_S)+len);
    if(NULL == subInfo)
    {
        aliyun_iot_mutex_unlock(&c->subInfoLock);
        WRITE_IOT_ERROR_LOG("run aliyun_iot_memory_malloc is error!");
        return FAIL_RETURN;
    }

    subInfo->nodeState = NODE_NORMANL_STATE;
    subInfo->msgId = msgId;
    subInfo->len = len;
    StartTimer(&subInfo->subTime);
    subInfo->type = type;
    subInfo->handler = *handler;
    subInfo->buf = (unsigned char*)subInfo + sizeof(SUBSCRIBE_INFO_S);

    //���Ʊ��������
    memcpy(subInfo->buf,c->buf,len);

    //����������
    *node = list_node_new(subInfo);
    if(NULL == *node)
    {
        aliyun_iot_mutex_unlock(&c->subInfoLock);
        WRITE_IOT_ERROR_LOG("run list_node_new is error!");
        return FAIL_RETURN;
    }

    //��������������
    list_rpush(c->subInfoList,*node);

    aliyun_iot_mutex_unlock(&c->subInfoLock);

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: mask_subInfo_from
* ��       ��: ɾ��subinfo��Ϣ
* �������: Client* c
*          unsigned int  msgId
*          MessageHandlers *messageHandler
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��: ɾ����ӦmsgId��subinfo��Ϣ���ڽ��յ�suback����ô˽ӿ�ɾ����Ӧsub��Ϣ
************************************************************/
int mask_subInfo_from(Client* c,unsigned int  msgId,MessageHandlers *messageHandler)
{
    aliyun_iot_mutex_lock(&c->subInfoLock);
    if (c->subInfoList->len)
    {
        list_iterator_t *iter = list_iterator_new(c->subInfoList, LIST_TAIL);
        list_node_t *node = NULL;
        SUBSCRIBE_INFO_S *subInfo = NULL;

        for (;;)
        {
            node = list_iterator_next(iter);
            if (NULL == node)
            {
                break;
            }

            subInfo = (SUBSCRIBE_INFO_S *) node->val;
            if (NULL == subInfo)
            {
                WRITE_IOT_ERROR_LOG("node's value is invalid!");
                continue;
            }

            if (subInfo->msgId == msgId)
            {
                //�˴�ɾ�������ظ���Ϣ������messageHandler��Ϊ����ֵ����һ��
                *messageHandler = subInfo->handler;

                //��Ǵ�ɾ�����
                subInfo->nodeState = NODE_INVALID_STATE;
            }
        }

        list_iterator_destroy(iter);
    }
    aliyun_iot_mutex_unlock(&c->subInfoLock);

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: NewMessageData
* ��       ��: �½���Ϣ����
* �������: MessageData* md
*          MQTTString* aTopicName
*          MQTTMessage* aMessgage
* �������: VOID
* �� ��  ֵ: VOID
* ˵      ��:
************************************************************/
void NewMessageData(MessageData* md, MQTTString* aTopicName, MQTTMessage* aMessgage)
{
    md->topicName = aTopicName;
    md->message = aMessgage;
}

/***********************************************************
* ��������: getNextPacketId
* ��       ��: ��ȡ���ı�ʶ��
* �������: Client *c
* �������: VOID
* �� ��  ֵ: id
* ˵      ��:
************************************************************/
int getNextPacketId(Client *c)
{
    unsigned int id = 0;
    aliyun_iot_mutex_lock(&c->idLock);
    c->next_packetid = (c->next_packetid == MAX_PACKET_ID) ? 1 : c->next_packetid + 1;
    id = c->next_packetid;
    aliyun_iot_mutex_unlock(&c->idLock);

    return id;
}

/***********************************************************
* ��������: sendPacket
* ��       ��: ����mqtt���Ĳ���
* �������: Client *c
*          int length
*          Timer* timer ���ͳ�ʱ��ʱ��
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int sendPacket(Client* c,unsigned char *buf,int length, Timer* timer)
{
    int rc = FAIL_RETURN;
    int sent = 0;
    
    while (sent < length && !expired(timer))
    {
        rc = c->ipstack->mqttwrite(c->ipstack, &buf[sent], length, left_ms(timer));
        if (rc < 0)  // there was an error writing the data
        {
            break;
        }
        sent += rc;
    }

    if (sent == length)
    {
        rc = SUCCESS_RETURN;
    }
    else
    {
        rc = MQTT_NETWORK_ERROR;
    }
    return rc;
}

/***********************************************************
* ��������: decodePacket
* ��       ��: ������ձ���
* �������: Client *c
*          int* value,
*          int timeout
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int decodePacket(Client* c, int* value, int timeout)
{
    unsigned char i;
    int multiplier = 1;
    int len = 0;
    const int MAX_NO_OF_REMAINING_LENGTH_BYTES = 4;

    *value = 0;
    do
    {
        int rc = MQTTPACKET_READ_ERROR;

        if (++len > MAX_NO_OF_REMAINING_LENGTH_BYTES)
        {
            return MQTTPACKET_READ_ERROR; /* bad data */
        }

        rc = c->ipstack->mqttread(c->ipstack, &i, 1, timeout);
        if (rc != 1)
        {
            return MQTT_NETWORK_ERROR;
        }

        *value += (i & 127) * multiplier;
        multiplier *= 128;
    } while ((i & 128) != 0);

    return len;
}

/***********************************************************
* ��������: readPacket
* ��       ��: ��ȡ����
* �������: Client*
*          Timer* timer
*          unsigned int *packet_type
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int readPacket(Client* c, Timer* timer,unsigned int *packet_type)
{
    MQTTHeader header = {0};
    int len = 0;
    int rem_len = 0;
    int rc = 0;
	int needReadLen;
	int remainDataLen;
	unsigned char *remainDataBuf = NULL;
    /* 1. read the header byte.  This has the packet type in it */
    if (c->ipstack->mqttread(c->ipstack, c->readbuf, 1, left_ms(timer)) != 1)
    {
        WRITE_IOT_DEBUG_LOG("mqttread error");
        return FAIL_RETURN;
    }

    len = 1;

    /* 2. read the remaining length.  This is variable in itself */
    if((rc = decodePacket(c, &rem_len, left_ms(timer))) < 0)
    {
        WRITE_IOT_ERROR_LOG("decodePacket error,rc = %d",rc);
        return rc;
    }

    len += MQTTPacket_encode(c->readbuf + 1, rem_len); /* put the original remaining length back into the buffer */

    /*Check if the received data length exceeds mqtt read buffer length*/
	if((rem_len > 0) && ((rem_len + len) > c->readbuf_size))
	{
		WRITE_IOT_ERROR_LOG("mqtt read buffer is too short, mqttReadBufLen : %zd, remainDataLen : %d", c->readbuf_size, rem_len);
		needReadLen = c->readbuf_size - len;
		if(c->ipstack->mqttread(c->ipstack, c->readbuf + len, needReadLen, left_ms(timer)) != needReadLen)
		{
			WRITE_IOT_ERROR_LOG("mqtt read error");
			return FAIL_RETURN;
		}

		/* drop data whitch over the length of mqtt buffer*/
		remainDataLen = rem_len - needReadLen;
		remainDataBuf = aliyun_iot_memory_malloc(remainDataLen + 1);
		if(!remainDataBuf)
		{
			WRITE_IOT_ERROR_LOG("malloc remain buffer failed");
			return FAIL_RETURN;
		}
		
		if(c->ipstack->mqttread(c->ipstack, remainDataBuf, remainDataLen, left_ms(timer)) != remainDataLen)
		{
			WRITE_IOT_ERROR_LOG("mqtt read error");
			aliyun_iot_memory_free(remainDataBuf);
			remainDataBuf = NULL;
			return FAIL_RETURN;
		}

		aliyun_iot_memory_free(remainDataBuf);
		remainDataBuf = NULL;

		return FAIL_RETURN;
		
	}
    /* 3. read the rest of the buffer using a callback to supply the rest of the data */
    if (rem_len > 0 && (c->ipstack->mqttread(c->ipstack, c->readbuf + len, rem_len, left_ms(timer)) != rem_len))
    {
        WRITE_IOT_ERROR_LOG("mqtt read error");
        return FAIL_RETURN;
    }

    header.byte = c->readbuf[0];
    *packet_type = header.bits.type;
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: isTopicMatched
* ��       ��: �����Ƿ�ƥ��ӿ�
* �������: char* topicFilter
*           MQTTString* topicName
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��: ������������ж��Ƿ�������ƥ��
************************************************************/
char isTopicMatched(char* topicFilter, MQTTString* topicName)
{
    char* curf = topicFilter;
    char* curn = topicName->lenstring.data;
    char* curn_end = curn + topicName->lenstring.len;
    
    while (*curf && curn < curn_end)
    {
        if (*curn == '/' && *curf != '/')
        {
            break;
        }

        if (*curf != '+' && *curf != '#' && *curf != *curn)
        {
            break;
        }

        if (*curf == '+')
        {   // skip until we meet the next separator, or end of string
            char* nextpos = curn + 1;
            while (nextpos < curn_end && *nextpos != '/')
            {
                nextpos = ++curn + 1;
            }
        }
        else if (*curf == '#')
        {
            curn = curn_end - 1;    // skip until end of string
        }
        curf++;
        curn++;
    }
    
    return (curn == curn_end) && (*curf == '\0');
}

/***********************************************************
* ��������: deliverMessage
* ��       ��: ִ��pub��Ϣ���ջص�
* �������: Client* c
*           MQTTString* topicName
*           MQTTMessage* message
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int deliverMessage(Client* c, MQTTString* topicName, MQTTMessage* message)
{
    int i;
    int rc = FAIL_RETURN;

    // we have to find the right message handler - indexed by topic
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if (c->messageHandlers[i].topicFilter != 0 && (MQTTPacket_equals(topicName, (char*)c->messageHandlers[i].topicFilter) || isTopicMatched((char*)c->messageHandlers[i].topicFilter, topicName)))
        {
            if (c->messageHandlers[i].fp != NULL)
            {
                MessageData md;
                NewMessageData(&md, topicName, message);
                c->messageHandlers[i].fp(&md);
                rc = SUCCESS_RETURN;
            }
        }
    }
    
    if (rc == FAIL_RETURN && c->defaultMessageHandler != NULL)
    {
        MessageData md;
        NewMessageData(&md, topicName, message);
        c->defaultMessageHandler(&md);
        rc = SUCCESS_RETURN;
    }   
    
    return rc;
}

/***********************************************************
* ��������: recvConnAckProc
* ��       ��: ����connect ack���Ĵ�����
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int recvConnAckProc(Client *c)
{
    int rc = SUCCESS_RETURN;
    unsigned char connack_rc = 255;
    char sessionPresent = 0;
    if (MQTTDeserialize_connack((unsigned char*)&sessionPresent, &connack_rc, c->readbuf, c->readbuf_size) != 1)
    {
        WRITE_IOT_ERROR_LOG("connect ack is error");
        return MQTT_CONNECT_ACK_PACKET_ERROR;
    }

    switch(connack_rc)
    {
        case CONNECTION_ACCEPTED:
            rc = SUCCESS_RETURN;
            break;
        case CONNECTION_REFUSED_UNACCEPTABLE_PROTOCOL_VERSION:
            rc = MQTT_CONANCK_UNACCEPTABLE_PROTOCOL_VERSION_ERROR;
            break;
        case CONNECTION_REFUSED_IDENTIFIER_REJECTED:
            rc = MQTT_CONNACK_IDENTIFIER_REJECTED_ERROR;
            break;
        case CONNECTION_REFUSED_SERVER_UNAVAILABLE:
            rc = MQTT_CONNACK_SERVER_UNAVAILABLE_ERROR;
            break;
        case CONNECTION_REFUSED_BAD_USERDATA:
            rc = MQTT_CONNACK_BAD_USERDATA_ERROR;
            break;
        case CONNECTION_REFUSED_NOT_AUTHORIZED:
            rc = MQTT_CONNACK_NOT_AUTHORIZED_ERROR;
            break;
        default:
            rc = MQTT_CONNACK_UNKNOWN_ERROR;
            break;
    }

    return rc;
}

/***********************************************************
* ��������: recvPubAckProc
* ��       ��: ����pub ack���Ĵ�����
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int recvPubAckProc(Client *c)
{
    unsigned short mypacketid;
    unsigned char dup = 0;
    unsigned char type = 0;

    if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
    {
        return MQTT_PUBLISH_ACK_PACKET_ERROR;
    }

    (void)mask_pubInfo_from(c,mypacketid);

    (void)aliyun_iot_sem_post(&c->semaphore);

    if(c->deliveryCompleteFun != NULL)
    {
        c->deliveryCompleteFun(NULL,mypacketid);
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: recvSubAckProc
* ��       ��: ����sub ack���Ĵ�����
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int recvSubAckProc(Client*c)
{
    int rc = FAIL_RETURN;
    unsigned short mypacketid;
    int count = 0, grantedQoS = -1;
	MessageHandlers messagehandler;
	int i;
    if (MQTTDeserialize_suback(&mypacketid, 1, &count, &grantedQoS, c->readbuf, c->readbuf_size) != 1)
    {
        WRITE_IOT_ERROR_LOG("Sub ack packet error");
        return MQTT_SUBSCRIBE_ACK_PACKET_ERROR;
    }

    if (grantedQoS == 0x80)
    {
        WRITE_IOT_ERROR_LOG("QOS of Sub ack packet error,grantedQoS = %d",grantedQoS);
        return MQTT_SUBSCRIBE_QOS_ERROR;
    }

    
    messagehandler.fp = NULL;
    messagehandler.topicFilter = NULL;
    (void)mask_subInfo_from(c,mypacketid,&messagehandler);

    if(messagehandler.fp == NULL || messagehandler.topicFilter == NULL)
    {
        return MQTT_SUB_INFO_NOT_FOUND_ERROR;
    }

    
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        /*If subscribe the same topic and callback function, then ignore*/
        if((NULL != c->messageHandlers[i].topicFilter) && (0 == whether_messagehandler_same(&c->messageHandlers[i],&messagehandler)))
        {
            rc = SUCCESS_RETURN;
            break;
        }
    }

    if(SUCCESS_RETURN != rc)
    {
        for(i = 0 ;i < MAX_MESSAGE_HANDLERS;++i)
        {
            if (NULL == c->messageHandlers[i].topicFilter)
            {
                c->messageHandlers[i].topicFilter = messagehandler.topicFilter;
                c->messageHandlers[i].fp = messagehandler.fp;
                break;
            }
        }
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: recvPublishProc
* ��       ��: ����pub���Ĵ�����
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int recvPublishProc(Client*c)
{
    int result = 0;
    MQTTString topicName;
	MQTTMessage msg;
    memset(&topicName,0x0,sizeof(topicName));
    
    memset(&msg,0x0,sizeof(msg));

    if (MQTTDeserialize_publish((unsigned char*)&msg.dup, (int*)&msg.qos, (unsigned char*)&msg.retained, (unsigned short*)&msg.id, &topicName,
       (unsigned char**)&msg.payload, (int*)&msg.payloadlen, c->readbuf, c->readbuf_size) != 1)
    {
        return MQTT_PUBLISH_PACKET_ERROR;
    }

    deliverMessage(c, &topicName, &msg);
    if (msg.qos == QOS0)
    {
        return SUCCESS_RETURN;
    }

    if (msg.qos == QOS1)
    {
        result = MQTTPuback(c,msg.id,PUBACK);
    }
    else if (msg.qos == QOS2)
    {
        result = MQTTPuback(c,msg.id,PUBREC);
    }
    else
    {
        WRITE_IOT_ERROR_LOG("Qos is error,qos = %d",msg.qos);
        return MQTT_PUBLISH_QOS_ERROR;
    }

    return result;
}

/***********************************************************
* ��������: recvPubRecProc
* ��       ��: ����pub rec������
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int recvPubRecProc(Client *c)
{
    unsigned short mypacketid;
    unsigned char dup = 0;
    unsigned char type = 0;
    int rc = 0;

    if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
    {
        return MQTT_PUBLISH_REC_PACKET_ERROR;
    }

    (void)mask_pubInfo_from(c,mypacketid);

    (void)aliyun_iot_sem_post(&c->semaphore);

    rc = MQTTPuback(c,mypacketid,PUBREL);
    if (rc == MQTT_NETWORK_ERROR)
    {
        return MQTT_NETWORK_ERROR;
    }

    return rc;
}


/***********************************************************
* ��������: recvPubCompProc
* ��       ��: ����pub comp������
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int recvPubCompProc(Client *c)
{
    unsigned short mypacketid;
    unsigned char dup = 0;
    unsigned char type = 0;

    if (MQTTDeserialize_ack(&type, &dup, &mypacketid, c->readbuf, c->readbuf_size) != 1)
    {
        return MQTT_PUBLISH_COMP_PACKET_ERROR;
    }

    if(c->deliveryCompleteFun != NULL)
    {
        c->deliveryCompleteFun(NULL,mypacketid);
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: recvUnsubAckProc
* ��       ��: ����unsub ack������
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int recvUnsubAckProc(Client *c)
{
    unsigned short mypacketid = 0;  // should be the same as the packetid above
	MessageHandlers messageHandler;
	/* Remove from message handler array */
    unsigned int i;
    if (MQTTDeserialize_unsuback(&mypacketid, c->readbuf, c->readbuf_size) != 1)
    {
        return MQTT_UNSUBSCRIBE_ACK_PACKET_ERROR;
    }

    
    (void)mask_subInfo_from(c,mypacketid,&messageHandler);

    
    for(i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
    {
        if((c->messageHandlers[i].topicFilter != NULL) && (0 == whether_messagehandler_same(&c->messageHandlers[i],&messageHandler)))
        {
            c->messageHandlers[i].topicFilter = NULL;
            c->messageHandlers[i].fp = NULL;
            /* We don't want to break here, in case the same topic is registered with 2 callbacks. Unlikely scenario */
        }
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: waitforConnack
* ��       ��: �ȴ�connect ack
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int waitforConnack(Client* c)
{
    unsigned int packetType = 0;
    int rc = 0;
    Timer timer;
    InitTimer(&timer);
    countdown_ms(&timer, c->connectdata.keepAliveInterval*1000);

    do
    {
        // read the socket, see what work is due
        rc = readPacket(c, &timer,&packetType);
        if(rc != SUCCESS_RETURN)
        {
            WRITE_IOT_ERROR_LOG("readPacket error,result = %d",rc);
            return MQTT_NETWORK_ERROR;
        }

    }while(packetType != CONNACK);

    rc = recvConnAckProc(c);
    if(SUCCESS_RETURN != rc)
    {
        WRITE_IOT_ERROR_LOG("recvConnackProc error,result = %d",rc);
    }

    return rc;
}

/***********************************************************
* ��������: cycle
* ��       ��: ����ѭ��
* �������: Client* c
*          Timer* timer
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
int cycle(Client* c, Timer* timer)
{
    unsigned int packetType;
    int rc = SUCCESS_RETURN;

    MQTTClientState state = aliyun_iot_mqtt_get_client_state(c);
    if(state != CLIENT_STATE_CONNECTED)
    {
        WRITE_IOT_DEBUG_LOG("state = %d",state);
        return MQTT_STATE_ERROR;
    }

    // read the socket, see what work is due
    rc = readPacket(c, timer,&packetType);
    if(rc != SUCCESS_RETURN)
    {
        aliyun_iot_mqtt_set_client_state(c, CLIENT_STATE_DISCONNECTED);
        WRITE_IOT_DEBUG_LOG("readPacket error,result = %d",rc);
        return MQTT_NETWORK_ERROR;
    }
    
    //receive any data to renew ping_timer
    countdown_ms(&c->ping_timer, c->connectdata.keepAliveInterval*1000);

    //���յ��κ����ݼ����ping��Ϣ��־
    aliyun_iot_mutex_lock(&c->pingMarkLock);
    c->pingMark = 0;
    aliyun_iot_mutex_unlock(&c->pingMarkLock);

    switch (packetType)
    {
        case CONNACK:
        {
            break;
        }
        case PUBACK:
        {
            rc = recvPubAckProc(c);
            if(SUCCESS_RETURN != rc)
            {
                WRITE_IOT_ERROR_LOG("recvPubackProc error,result = %d",rc);
            }
            break;
        }
        case SUBACK:
        {
            rc = recvSubAckProc(c);
            if(SUCCESS_RETURN != rc)
            {
                WRITE_IOT_ERROR_LOG("recvSubAckProc error,result = %d",rc);
            }
            break;
        }
        case PUBLISH:
        {
            rc = recvPublishProc(c);
            if(SUCCESS_RETURN != rc)
            {
                WRITE_IOT_ERROR_LOG("recvPublishProc error,result = %d",rc);
            }
            break;
        }
        case PUBREC:
        {
            rc = recvPubRecProc(c);
            if(SUCCESS_RETURN != rc)
            {
                WRITE_IOT_ERROR_LOG("recvPubRecProc error,result = %d",rc);
            }
            break;
        }
        case PUBCOMP:
        {
            rc = recvPubCompProc(c);
            if(SUCCESS_RETURN != rc)
            {
                WRITE_IOT_ERROR_LOG("recvPubCompProc error,result = %d",rc);
            }
            break;
        }
        case UNSUBACK:
        {
            rc = recvUnsubAckProc(c);
            if(SUCCESS_RETURN != rc)
            {
                WRITE_IOT_ERROR_LOG("recvUnsubAckProc error,result = %d",rc);
            }
            break;
        }
        case PINGRESP:
        {
            rc = SUCCESS_RETURN;
            WRITE_IOT_INFO_LOG("receive ping response!");
            break;
        }
        default:
            return FAIL_RETURN;
    }

    return rc;
}

/***********************************************************
* ��������: whether_mqtt_client_state_normal
* ��       ��: �ж�mqtt client״̬�Ƿ�����
* �������: Client* c
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵      ��:
************************************************************/
BOOL whether_mqtt_client_state_normal(Client*c)
{
    if(aliyun_iot_mqtt_get_client_state(c) == CLIENT_STATE_CONNECTED)
    {
        return 1;
    }

    return 0;
}

/***********************************************************
* ��������: whether_messagehandler_same
* ��       ��: �ж϶��ĵ�messagehandler�Ƿ���ͬ
* �������: MessageHandlers *messageHandlers1
*          MessageHandlers *messageHandler2
* �������: int
* �� ��  ֵ: 0����ͬ  ��0������ͬ
* ˵      ��:
************************************************************/
int whether_messagehandler_same(MessageHandlers *messageHandlers1,MessageHandlers *messageHandler2)
{
    int topicNameLen = strlen(messageHandlers1->topicFilter);

    if(topicNameLen != strlen(messageHandler2->topicFilter))
    {
        return 1;
    }

    if(strncmp(messageHandlers1->topicFilter, messageHandler2->topicFilter, topicNameLen) != 0)
    {
        return 1;
    }

    if(messageHandlers1->fp != messageHandler2->fp)
    {
        return 1;
    }

    return 0;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_subscribe
* ��       ��: mqtt������Ϣ
* �������: Client* c                     mqtt�ͻ���
*          char* topicFilter             ���ĵ��������
*          enum QoS qos                  ��Ϣ��������
*          messageHandler messageHandler ��Ϣ����ṹ
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: MQTT���Ĳ��������ĳɹ������յ�sub ack
************************************************************/
int aliyun_iot_mqtt_subscribe(Client* c, const char* topicFilter, enum QoS qos, messageHandler messageHandler)
{ 
	int rc = FAIL_RETURN;
	unsigned int msgId;
    if(NULL == c || NULL == topicFilter)
    {
        return NULL_VALUE_ERROR;
    }

    
    
    if (!whether_mqtt_client_state_normal(c))
    {
        WRITE_IOT_ERROR_LOG("mqtt client state is error,state = %d",aliyun_iot_mqtt_get_client_state(c));
        return MQTT_STATE_ERROR;
    }
    
    if(0 != aliyun_iot_common_check_topic(topicFilter,TOPIC_FILTER_TYPE))
    {
        WRITE_IOT_ERROR_LOG("topic format is error,topicFilter = %s",topicFilter);
        return MQTT_TOPIC_FORMAT_ERROR;
    }

    msgId = getNextPacketId(c);
    rc = MQTTSubscribe(c,topicFilter, qos,msgId,messageHandler);
    if (rc != SUCCESS_RETURN)
    {
        if(rc == MQTT_NETWORK_ERROR)
        {
            aliyun_iot_mqtt_set_client_state(c, CLIENT_STATE_DISCONNECTED);
        }

        WRITE_IOT_ERROR_LOG("run MQTTSubscribe error");
        return rc;
    }

    WRITE_IOT_NOTICE_LOG("mqtt subscribe success,topic = %s!",topicFilter);
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_sub_sync
* ��       ��: mqttͬ������ACK
* �������: Client* c  mqtt�ͻ���
*          char* topicFilter ���ĵ��������
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ͬsubscribe
************************************************************/
int aliyun_iot_mqtt_suback_sync(Client* c,const char* topicFilter,messageHandler messageHandler)
{
    int rc = SUCCESS_RETURN;
	list_iterator_t *iter;
	list_node_t *node = NULL;
	SUBSCRIBE_INFO_S *subInfo = NULL;
    aliyun_iot_mutex_lock(&c->subInfoLock);
    do
    {
        if(0 == c->subInfoList->len)
        {
            rc = SUCCESS_RETURN;
            break;
        }

        iter = list_iterator_new(c->subInfoList, LIST_TAIL);
        

        for (;;)
        {
            node = list_iterator_next(iter);

            if (NULL == node)
            {
                //��ѯ����
                break;
            }

            subInfo = (SUBSCRIBE_INFO_S *) node->val;
            if (NULL == subInfo)
            {
                continue;
            }

            //��Ч�ڵ���Ѱ����һ��
            if(NODE_INVALID_STATE == subInfo->nodeState)
            {
                continue;
            }

            if((0 == strncmp(subInfo->handler.topicFilter,topicFilter,strlen(topicFilter))) && (messageHandler == subInfo->handler.fp))
            {
                rc = FAIL_RETURN;
                break;
            }
        }

        list_iterator_destroy(iter);

    }while(0);

    aliyun_iot_mutex_unlock(&c->subInfoLock);

    return rc;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_unsubscribe
* ��       ��: mqttȡ������
* �������: Client* c  mqtt�ͻ���
*          char* topicFilter ���ĵ��������
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ͬsubscribe
************************************************************/
int aliyun_iot_mqtt_unsubscribe(Client* c, const char* topicFilter)
{   
	int rc = FAIL_RETURN;
	unsigned int msgId;
    if(NULL == c || NULL == topicFilter)
    {
        return NULL_VALUE_ERROR;
    }

    if(0 != aliyun_iot_common_check_topic(topicFilter,TOPIC_FILTER_TYPE))
    {
        WRITE_IOT_ERROR_LOG("topic format is error,topicFilter = %s",topicFilter);
        return MQTT_TOPIC_FORMAT_ERROR;
    }

    
    
    if (!whether_mqtt_client_state_normal(c))
    {
        WRITE_IOT_ERROR_LOG("mqtt client state is error,state = %d",aliyun_iot_mqtt_get_client_state(c));
        return MQTT_STATE_ERROR;
    }
    
    msgId = getNextPacketId(c);

    rc = MQTTUnsubscribe(c,topicFilter, msgId);
    if(rc != SUCCESS_RETURN)
    {
        if (rc == MQTT_NETWORK_ERROR) // send the subscribe packet
        {
            aliyun_iot_mqtt_set_client_state(c, CLIENT_STATE_DISCONNECTED);
        }

        WRITE_IOT_ERROR_LOG("run MQTTUnsubscribe error!");
        return rc;
    }
    
    WRITE_IOT_NOTICE_LOG("mqtt unsubscribe success,topic = %s!",topicFilter);
    return SUCCESS_RETURN;
}

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
int aliyun_iot_mqtt_publish(Client* c, const char* topicName, MQTTMessage* message)
{
	int rc = FAIL_RETURN;
    if(NULL == c || NULL == topicName || NULL == message)
    {
        return NULL_VALUE_ERROR;
    }

    if(0 != aliyun_iot_common_check_topic(topicName,TOPIC_NAME_TYPE))
    {
        WRITE_IOT_ERROR_LOG("topic format is error,topicFilter = %s",topicName);
        return MQTT_TOPIC_FORMAT_ERROR;
    }

    if (!whether_mqtt_client_state_normal(c))
    {
        WRITE_IOT_ERROR_LOG("mqtt client state is error,state = %d",aliyun_iot_mqtt_get_client_state(c));
        return MQTT_STATE_ERROR;
    }

    if (message->qos == QOS1 || message->qos == QOS2)
    {
        message->id = getNextPacketId(c);
    }
    
    rc = MQTTPublish(c,topicName, message);
    if (rc != SUCCESS_RETURN) // send the subscribe packet
    {
        if(rc == MQTT_NETWORK_ERROR)
        {
            aliyun_iot_mqtt_set_client_state(c, CLIENT_STATE_DISCONNECTED);
        }
        WRITE_IOT_ERROR_LOG("MQTTPublish is error,rc = %d",rc);
        return rc;
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_get_client_state
* ��       ��: ��ȡmqtt�ͻ���״̬
* �������: Client* c  mqtt�ͻ���
* �������: VOID
* �� ��  ֵ: mqtt client״̬
* ˵       ��:
************************************************************/
MQTTClientState aliyun_iot_mqtt_get_client_state(Client *pClient)
{
	MQTTClientState state;
	IOT_FUNC_ENTRY;
	aliyun_iot_mutex_lock(&pClient->stateLock);
	state = pClient->clientState;
	aliyun_iot_mutex_unlock(&pClient->stateLock);

	return state;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_set_client_state
* ��       ��: ����mqtt�ͻ���״̬
* �������: Client* c  mqtt�ͻ���
* �������: VOID
* �� ��  ֵ: mqtt client״̬
* ˵       ��:
************************************************************/
void aliyun_iot_mqtt_set_client_state(Client *pClient, MQTTClientState newState)
{
	IOT_FUNC_ENTRY;
	
	aliyun_iot_mutex_lock(&pClient->stateLock);
	pClient->clientState = newState;
	aliyun_iot_mutex_unlock(&pClient->stateLock);
}

/***********************************************************
* ��������: aliyun_iot_mqtt_set_connect_params
* ��       ��: ����mqtt���Ӳ���
* �������: Client* c  mqtt�ͻ���
*           MQTTPacket_connectData *pConnectParams
* �������: VOID
* �� ��  ֵ: mqtt client״̬
* ˵       ��:
************************************************************/
int aliyun_iot_mqtt_set_connect_params(Client *pClient, MQTTPacket_connectData *pConnectParams)
{

	IOT_FUNC_ENTRY;
	
	if(NULL == pClient || NULL == pConnectParams) 
	{
		IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
	}

	memcpy(pClient->connectdata.struct_id , pConnectParams->struct_id , 4);
	pClient->connectdata.struct_version = pConnectParams->struct_version;
	pClient->connectdata.MQTTVersion = pConnectParams->MQTTVersion;
	pClient->connectdata.clientID = pConnectParams->clientID;
	pClient->connectdata.cleansession = pConnectParams->cleansession;
	pClient->connectdata.willFlag = pConnectParams->willFlag;
	pClient->connectdata.username = pConnectParams->username;
	pClient->connectdata.password = pConnectParams->password;
	memcpy(pClient->connectdata.will.struct_id , pConnectParams->will.struct_id, 4);
	pClient->connectdata.will.struct_version = pConnectParams->will.struct_version;
	pClient->connectdata.will.topicName = pConnectParams->will.topicName;
	pClient->connectdata.will.message = pConnectParams->will.message;
	pClient->connectdata.will.qos = pConnectParams->will.qos;
	pClient->connectdata.will.retained = pConnectParams->will.retained;

	if(pConnectParams->keepAliveInterval < KEEP_ALIVE_INTERVAL_DEFAULT_MIN)
	{
	    pClient->connectdata.keepAliveInterval = KEEP_ALIVE_INTERVAL_DEFAULT_MIN;
	}
	else if(pConnectParams->keepAliveInterval > KEEP_ALIVE_INTERVAL_DEFAULT_MAX)
	{
	    pClient->connectdata.keepAliveInterval = KEEP_ALIVE_INTERVAL_DEFAULT_MAX;
	}
	else
	{
		pClient->connectdata.keepAliveInterval = pConnectParams->keepAliveInterval;
	}

	IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}

/***********************************************************
* ��������: aliyun_iot_mqtt_init
* ��       ��: mqtt��ʼ��
* �������: Client *pClient
*          IOT_CLIENT_INIT_PARAMS *pInitParams
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: �û�����Client�ṹ���˽ӿڸ����û��ĳ�ʼ��������ʼ��Client�ṹ
************************************************************/
int aliyun_iot_mqtt_init(Client *pClient, IOT_CLIENT_INIT_PARAMS *pInitParams)
{
	int rc = FAIL_RETURN;
	int i = 0;
	MQTTPacket_connectData connectdata = MQTTPacket_connectData_initializer;
	IOT_FUNC_ENTRY;

    if((NULL == pClient) || (NULL == pInitParams))
    {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

	

	memset(pClient,0x0,sizeof(Client));

	//connectdata = MQTTPacket_connectData_initializer;
	
	    
    for (i = 0; i < MAX_MESSAGE_HANDLERS; ++i)
	{
        pClient->messageHandlers[i].topicFilter = NULL;
		pClient->messageHandlers[i].fp = NULL;
    }

	pClient->next_packetid = 0;
	aliyun_iot_mutex_init(&pClient->idLock);
	aliyun_iot_mutex_init(&pClient->subInfoLock);
	aliyun_iot_mutex_init(&pClient->pubInfoLock);

	//��Ϣ���ͳ�ʱʱ��
	if(pInitParams->mqttCommandTimeout_ms < ALI_IOT_MQTT_MIN_COMMAND_TIMEOUT || pInitParams->mqttCommandTimeout_ms > ALI_IOT_MQTT_MAX_COMMAND_TIMEOUT)
	{
	    pClient->command_timeout_ms = ALI_IOT_MQTT_DEFAULT_COMMAND_TIMEOUT;
	}
	else
	{
	    pClient->command_timeout_ms = pInitParams->mqttCommandTimeout_ms;
	}

    pClient->buf = pInitParams->pWriteBuf;
    pClient->buf_size = pInitParams->writeBufSize;
    pClient->readbuf = pInitParams->pReadBuf;
    pClient->readbuf_size = pInitParams->readBufSize;
    pClient->defaultMessageHandler = NULL;
    pClient->deliveryCompleteFun = pInitParams->deliveryCompleteFun;
    pClient->subAckTimeOutFun = pInitParams->subAckTimeOutFun;
    aliyun_iot_mutex_init(&pClient->stateLock);
    aliyun_iot_mutex_init(&pClient->pingMarkLock);

	/*reconnect params init*/
	pClient->reconnectparams.disconnectHandler = pInitParams->disconnectHandler;
	pClient->reconnectparams.disconnectHandlerData = pInitParams->disconnectHandlerData;
	pClient->reconnectparams.reconnectHandler = pInitParams->reconnectHandler;
	pClient->reconnectparams.reconnectHandlerData = pInitParams->reconnectHandlerData;
	pClient->reconnectparams.currentReconnectWaitInterval = ALI_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL;
	pClient->reconnectparams.reconnectNum = 0;
    memset(&pClient->recieveThread,0x0,sizeof(ALIYUN_IOT_PTHREAD_S));
    memset(&pClient->keepaliveThread,0x0,sizeof(ALIYUN_IOT_PTHREAD_S));
    memset(&pClient->retransThread,0x0,sizeof(ALIYUN_IOT_PTHREAD_S));

    pClient->pubInfoList = list_new();
    pClient->pubInfoList->free = aliyun_iot_memory_free;
    pClient->subInfoList = list_new();
    pClient->subInfoList->free = aliyun_iot_memory_free;

    aliyun_iot_mutex_init(&pClient->writebufLock);
    networkRecoverSignalInit(&pClient->networkRecoverSignal);

    (void)aliyun_iot_sem_init(&pClient->semaphore);

	/*init mqtt connect params*/
	rc = aliyun_iot_mqtt_set_connect_params(pClient, &connectdata);
	if(SUCCESS_RETURN != rc)
	{
	    aliyun_iot_mqtt_set_client_state(pClient,CLIENT_STATE_INVALID);
		IOT_FUNC_EXIT_RC(rc);
	}
	
    InitTimer(&pClient->ping_timer);
	InitTimer(&pClient->reconnectparams.reconnectDelayTimer);

	pClient->ipstack = (Network*)aliyun_iot_memory_malloc(sizeof(Network));
	if(NULL == pClient->ipstack)
	{
		WRITE_IOT_ERROR_LOG("malloc Network failed");
		IOT_FUNC_EXIT_RC(FAIL_RETURN);
	}
	memset(pClient->ipstack,0x0,sizeof(Network));

    rc = aliyun_iot_mqtt_network_init(pClient->ipstack, g_userInfo.hostAddress, g_userInfo.port, g_authInfo->pubkey);
    if(SUCCESS_RETURN != rc)
    {
        aliyun_iot_mqtt_set_client_state(pClient,CLIENT_STATE_INVALID);
        IOT_FUNC_EXIT_RC(rc);
    }

	aliyun_iot_mqtt_set_client_state(pClient,CLIENT_STATE_INITIALIZED);
	pClientStatus = pClient;
	WRITE_IOT_NOTICE_LOG("mqtt init success!");
	IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}

/***********************************************************
* ��������: aliyun_iot_receive_thread
* ��       ��: �����߳�
* �������: void * param
* �������: VOID
* �� ��  ֵ: VOID*
* ˵       ��:
************************************************************/
void* aliyun_iot_receive_thread(void * param)
{
	Client* pClient = NULL;
    WRITE_IOT_NOTICE_LOG("start the receive thread!");
    aliyun_iot_pthread_setname("iot_receive_thread");
    pClient = (Client* )param;

    for(;;)
    {
        aliyun_iot_mqtt_yield(pClient,pClient->connectdata.keepAliveInterval*1000 + PINGRSP_TIMEOUT_MS);
    }

    WRITE_IOT_NOTICE_LOG("end the receive thread!");
    return NULL;
}

/***********************************************************
* ��������: MQTTSubInfoProc
* ��       ��: sub ��Ϣ��ȴ���Ӧ�Ĵ���
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: VOID*
* ˵       ��:
************************************************************/
int MQTTSubInfoProc(Client *pClient)
{
    int rc = SUCCESS_RETURN;
	list_iterator_t *iter;
	list_node_t *node = NULL;
    list_node_t *tempNode = NULL;
	SUBSCRIBE_INFO_S *subInfo = NULL;
	MQTTClientState state;
    aliyun_iot_mutex_lock(&pClient->subInfoLock);
    do
    {
        if(0 == pClient->subInfoList->len)
        {
            break;
        }

        iter = list_iterator_new(pClient->subInfoList, LIST_TAIL);
        

        for (;;)
        {
            node = list_iterator_next(iter);

            if (NULL != tempNode)
            {
                list_remove(pClient->subInfoList, tempNode);
                tempNode = NULL;
            }

            if (NULL == node)
            {
                //��ѯ����
                break;
            }

            subInfo = (SUBSCRIBE_INFO_S *) node->val;
            if (NULL == subInfo)
            {
                WRITE_IOT_ERROR_LOG("node's value is invalid!");
                tempNode = node;
                continue;
            }

            //��Ч���ֱ��ɾ��
            if(NODE_INVALID_STATE == subInfo->nodeState)
            {
                tempNode = node;
                continue;
            }

            //״̬�쳣�����ѭ��
            state = aliyun_iot_mqtt_get_client_state(pClient);
            if(state != CLIENT_STATE_CONNECTED)
            {
                continue;
            }

            //�жϽڵ��Ƿ�ʱ
            if(spend_ms(&subInfo->subTime) <= (pClient->command_timeout_ms*2))
            {
                //û�г�ʱ�������һ���ڵ�
                continue;
            }

            aliyun_iot_mutex_unlock(&pClient->subInfoLock);

            //�ص���������
            if(pClient->subAckTimeOutFun != NULL)
            {
                pClient->subAckTimeOutFun(subInfo);
            }

            aliyun_iot_mutex_lock(&pClient->subInfoLock);

            tempNode = node;
        }

        list_iterator_destroy(iter);

    }while(0);

    aliyun_iot_mutex_unlock(&pClient->subInfoLock);

    return rc;
}

/***********************************************************
* ��������: aliyun_iot_keepalive_thread
* ��       ��: �����߳�
* �������: void * param
* �������: VOID
* �� ��  ֵ: VOID*
* ˵       ��:
************************************************************/
void* aliyun_iot_keepalive_thread(void * param)
{
	Client* pClient = (Client* )param;
	Timer timer;
	int rc = 0;
	unsigned int cycTime;
	int remainTime;
	MQTTClientState currentState;
    WRITE_IOT_NOTICE_LOG("start the keep alive thread!");
    

    aliyun_iot_pthread_setname("iot_keepalive_thread");

    //the time of keep alive fail
    countdown_ms(&pClient->ping_timer, pClient->connectdata.keepAliveInterval*1000);

    //Cycle time
    
    InitTimer(&timer);
    

    cycTime = pClient->reconnectparams.currentReconnectWaitInterval;
    if(pClient->reconnectparams.currentReconnectWaitInterval > pClient->connectdata.keepAliveInterval*1000)
    {
        cycTime = pClient->connectdata.keepAliveInterval*1000;
    }

    for(;;)
    {
        //set the cycle time
        countdown_ms(&timer, cycTime);

        /*Periodic sending ping packet to detect whether the network is connected*/
        (void)aliyun_iot_mqtt_keep_alive(pClient);

        currentState = aliyun_iot_mqtt_get_client_state(pClient);
        do
        {
            /*if Exceeds the maximum delay time, then return reconnect timeout*/
             if(CLIENT_STATE_DISCONNECTED_RECONNECTING == currentState)
             {
                 /*Reconnection is successful, Resume regularly ping packets*/
                 rc = aliyun_iot_mqtt_handle_reconnect(pClient);
                 if(SUCCESS_RETURN != rc)
                 {
                     //����ʧ�����Ӽ���
                     WRITE_IOT_DEBUG_LOG("reconnect network fail, rc = %d",rc);
                 }
                 else
                 {
                     WRITE_IOT_NOTICE_LOG("network is reconnected!");
					 
					  aliyun_iot_mutex_lock(&pClient->pingMarkLock);
					  pClient->pingMark = 0;
					  aliyun_iot_mutex_unlock(&pClient->pingMarkLock);
                     //�����ɹ���������ָ��ص�����
                     aliyun_iot_mqtt_reconnect_callback(pClient);

                     pClient->reconnectparams.currentReconnectWaitInterval = ALI_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL;
                 }

                 break;
             }

             /*If network suddenly interrupted, stop pinging packet,  try to reconnect network immediately*/
             if(CLIENT_STATE_DISCONNECTED == currentState)
             {
                 WRITE_IOT_ERROR_LOG("network is disconnected!");

                 //�����쳣���������Ͽ����ӻص�����
                 aliyun_iot_mqtt_disconnect_callback(pClient);

                 pClient->reconnectparams.currentReconnectWaitInterval = ALI_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL;
                 countdown_ms(&(pClient->reconnectparams.reconnectDelayTimer), pClient->reconnectparams.currentReconnectWaitInterval);

                 //�Ͽ�socket
                 pClient->ipstack->disconnect(pClient->ipstack);

                 aliyun_iot_mqtt_set_client_state(pClient, CLIENT_STATE_DISCONNECTED_RECONNECTING);

                 break;
             }

        }while(0);

        remainTime = left_ms(&timer);
        if(remainTime > 0)
        {
            aliyun_iot_pthread_taskdelay(remainTime);
        }
    }

    WRITE_IOT_NOTICE_LOG("end the keep alive thread!");
    return NULL;
}

/***********************************************************
* ��������: MQTTRePublish
* ��       ��: pub��Ϣ�ط�
* �������: Client*c
*           unsigned char*buf
*           int len
* �������:
* �� ��  ֵ: 0�ɹ�����0ʧ��
* ˵       ��:
************************************************************/
int MQTTRePublish(Client*c,unsigned char*buf,int len)
{
    Timer timer;
    InitTimer(&timer);
    countdown_ms(&timer, c->command_timeout_ms);

    if (sendPacket(c, buf,len, &timer) != SUCCESS_RETURN)
    {
        return MQTT_NETWORK_ERROR;
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: MQTTPubInfoProc
* ��       ��: pub ��Ϣ��Ӧ��Ի������ݵĴ���
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: VOID*
* ˵       ��: ��Ϊ�ط���״̬δ��֪�������������ʱ������ɾ������
*          ���ش������ͳһ�̲߳��ܱ�֤ɾ����Դʱû�������߳���ʹ��
************************************************************/
int MQTTPubInfoProc(Client *pClient)
{
    int rc = 0;
	list_iterator_t *iter;
	list_node_t *node = NULL;
    list_node_t *tempNode = NULL;
	REPUBLISH_INFO_S *repubInfo = NULL;
    MQTTClientState state = CLIENT_STATE_INVALID;

    aliyun_iot_mutex_lock(&pClient->pubInfoLock);
    do
    {
        if(0 == pClient->pubInfoList->len)
        {
            break;
        }

        iter = list_iterator_new(pClient->pubInfoList, LIST_TAIL);
        

        for (;;)
        {
            node = list_iterator_next(iter);

            if (NULL != tempNode)
            {
                list_remove(pClient->pubInfoList, tempNode);
                tempNode = NULL;
            }

            if (NULL == node)
            {
                //��ѯ����
                break;
            }

            repubInfo = (REPUBLISH_INFO_S *) node->val;
            if (NULL == repubInfo)
            {
                WRITE_IOT_ERROR_LOG("node's value is invalid!");
                tempNode = node;
                continue;
            }

            //����Ч�ڵ���ֱ��ɾ��
            if(NODE_INVALID_STATE == repubInfo->nodeState)
            {
                tempNode = node;
                continue;
            }

            //״̬�쳣�˳�ѭ��(״̬�жϲ������⻷��ԭ�����ڶ�������������Ȼ����ɾ����ЧpubInfo���)
            state = aliyun_iot_mqtt_get_client_state(pClient);
            if(state != CLIENT_STATE_CONNECTED)
            {
                continue;
            }

            //�жϽڵ��Ƿ�ʱ
            if(spend_ms(&repubInfo->pubTime) <= (pClient->command_timeout_ms*2))
            {
                //û�г�ʱ�������һ���ڵ�
                continue;
            }

            //����Ϊ��ʱ�ط�
            aliyun_iot_mutex_unlock(&pClient->pubInfoLock);
            rc = MQTTRePublish(pClient,repubInfo->buf,repubInfo->len);
            StartTimer(&repubInfo->pubTime);
            aliyun_iot_mutex_lock(&pClient->pubInfoLock);

            if(MQTT_NETWORK_ERROR == rc)
            {
                aliyun_iot_mqtt_set_client_state(pClient, CLIENT_STATE_DISCONNECTED);
                break;
            }
        }

        list_iterator_destroy(iter);

    }while(0);

    aliyun_iot_mutex_unlock(&pClient->pubInfoLock);

    return SUCCESS_RETURN;
}

void* aliyun_iot_retrans_thread(void*param)
{
	Client* pClient = (Client* )param;
    WRITE_IOT_NOTICE_LOG("start the retrans thread!");
    
    aliyun_iot_pthread_setname("iot_retrans_thread");

    for(;;)
    {
        if(FAIL_RETURN == aliyun_iot_sem_gettimeout(&pClient->semaphore,500))
        {
            continue;
        }

        //�����pub ack��ɾ��������pub list�е���Ϣ��QOS=1ʱ��
        (void)MQTTPubInfoProc(pClient);

        //�����sub ack��ɾ��������sub list�е���Ϣ
        (void)MQTTSubInfoProc(pClient);
    }

    return NULL;
}

/***********************************************************
* ��������: aliyun_iot_create_thread
* ��       ��: �����߳�
* �������: Client* pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int aliyun_iot_create_thread(Client* pClient)
{
	int result;
    if(0 != pClient->threadRunning)
    {
        return SUCCESS_RETURN;
    }

    result = aliyun_iot_pthread_create(&pClient->recieveThread,aliyun_iot_receive_thread,pClient,NULL);
    if(0 != result)
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_pthread_create error!");
        return FAIL_RETURN;
    }

    result = aliyun_iot_pthread_create(&pClient->keepaliveThread,aliyun_iot_keepalive_thread,pClient,NULL);
    if(0 != result)
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_pthread_create error!");
        aliyun_iot_pthread_cancel(&pClient->recieveThread);
        return FAIL_RETURN;
    }

    result = aliyun_iot_pthread_create(&pClient->retransThread,aliyun_iot_retrans_thread,pClient,NULL);
    if(0 != result)
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_retrans_thread error!");
        aliyun_iot_pthread_cancel(&pClient->recieveThread);
        aliyun_iot_pthread_cancel(&pClient->keepaliveThread);
        return FAIL_RETURN;
    }

    WRITE_IOT_NOTICE_LOG("create recieveThread, keepaliveThread and retransThread!");
    pClient->threadRunning = 1;
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_delete_thread
* ��       ��: ɾ���߳�
* �������: Client* pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int aliyun_iot_delete_thread(Client* pClient)
{
	int result;
    if(0 == pClient->threadRunning)
    {
        return SUCCESS_RETURN;
    }

    result = aliyun_iot_pthread_cancel(&pClient->recieveThread);
    if(0 != result)
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_pthread_cancel error!");
        return FAIL_RETURN;
    }
    memset(&pClient->recieveThread,0x0,sizeof(ALIYUN_IOT_PTHREAD_S));

    result = aliyun_iot_pthread_cancel(&pClient->keepaliveThread);
    if(0 != result)
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_pthread_cancel error!");
        return FAIL_RETURN;
    }
    memset(&pClient->keepaliveThread,0x0,sizeof(ALIYUN_IOT_PTHREAD_S));

    result = aliyun_iot_pthread_cancel(&pClient->retransThread);
    if(0 != result)
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_pthread_cancel error!");
        return FAIL_RETURN;
    }
    memset(&pClient->retransThread,0x0,sizeof(ALIYUN_IOT_PTHREAD_S));

    pClient->threadRunning = 0;
    WRITE_IOT_NOTICE_LOG("delete recieveThread, keepaliveThread and retransThread!");
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_connect
* ��       ��: mqttЭ������
* �������: Client *pClient
*           MQTTPacket_connectData* pConnectParams ���ӱ��Ĳ���
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ʵ������connect��mqttЭ���connect
************************************************************/
int aliyun_iot_mqtt_connect(Client* pClient, MQTTPacket_connectData* pConnectParams)
{
	int rc = FAIL_RETURN;
	IOT_FUNC_ENTRY;
    

    if((NULL == pClient) || (NULL == pConnectParams))
    {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    //�����û���Ϣ��ʼ�����Ӳ���
    pConnectParams->clientID.cstring  = g_userInfo.clientId;
    pConnectParams->username.cstring  = g_userInfo.userName;
    pConnectParams->password.cstring  = NULL;

    // don't send connect packet again if we are already connected
    if (whether_mqtt_client_state_normal(pClient))
    {
        WRITE_IOT_INFO_LOG("already connected!");
        return SUCCESS_RETURN;
    }

    aliyun_iot_mqtt_set_connect_params(pClient, pConnectParams);

	/*Establish TCP or TLS connection*/
	rc = pClient->ipstack->mqttConnect(pClient->ipstack);
	if(SUCCESS_RETURN != rc)
	{
        pClient->ipstack->disconnect(pClient->ipstack);
        WRITE_IOT_ERROR_LOG("TCP or TLS Connection failed");

	    if(CERTIFICATE_EXPIRED == rc)
	    {
	        WRITE_IOT_ERROR_LOG("certificate is expired!");
	        return ERROR_CERT_VERIFY_FAIL;
	    }
	    else
	    {
	        return MQTT_NETWORK_CONNECT_ERROR;
	    }
	}

	rc = MQTTConnect(pClient);
    if (rc  != SUCCESS_RETURN)
    {
        pClient->ipstack->disconnect(pClient->ipstack);
        WRITE_IOT_ERROR_LOG("send connect packet failed");
        return rc;
    }

    if(SUCCESS_RETURN != waitforConnack(pClient))
    {
        (void)MQTTDisconnect(pClient);
        pClient->ipstack->disconnect(pClient->ipstack);
        WRITE_IOT_ERROR_LOG("mqtt waitfor connect ack timeout!");
        return MQTT_CONNECT_ERROR;
    }

    rc = aliyun_iot_create_thread(pClient);
    if(SUCCESS_RETURN != rc)
    {
        //�����߳����ʧ����û�������߳̾��������Կ��Ե���disconnect�ӿ�
        (void)MQTTDisconnect(pClient);
        pClient->ipstack->disconnect(pClient->ipstack);
        return MQTT_CREATE_THREAD_ERROR;
    }

    aliyun_iot_mqtt_set_client_state(pClient, CLIENT_STATE_CONNECTED);

    WRITE_IOT_NOTICE_LOG("mqtt connect success!");
    return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_attempt_reconnect
* ��       ��: mqtt����
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int aliyun_iot_mqtt_attempt_reconnect(Client *pClient)
{
	int rc;
	IOT_FUNC_ENTRY;

	

	WRITE_IOT_NOTICE_LOG("reconnect params:MQTTVersion =%d clientID =%s keepAliveInterval =%d username = %s",
		pClient->connectdata.MQTTVersion,
		pClient->connectdata.clientID.cstring,
		pClient->connectdata.keepAliveInterval,
		pClient->connectdata.username.cstring);
	
	WRITE_IOT_NOTICE_LOG("reconnectNum = %d ",pClient->reconnectparams.reconnectNum);

	//������ȡIP
	if(pClient->reconnectparams.reconnectNum > CHANGE_IP_RECONNECT_NUM_MAX)
	{
	    //ֻ������ȡIP
	    if(0 != (rc = pull_server_param(g_authInfo->signDataType,NETWORK_SERVER_PARAM,g_authInfo)))
	    {
	        WRITE_IOT_ERROR_LOG("run pull_server_param() error!");
	        return rc;
	    }

        //��֤��������ĺϷ���
        if(SUCCESS_RETURN != aliyun_iot_verify_certificate(g_authInfo,NETWORK_SERVER_PARAM,g_authInfo->signDataType))
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_verify_certificate() error!");
            return FAIL_RETURN;
        }

	    pClient->reconnectparams.reconnectNum = 0;

	    //����ȡ����network��Ϣ����usrinfo�ڴ�
	    if(0 != set_usr_info(g_authInfo))
	    {
	        WRITE_IOT_ERROR_LOG("run set_usr_info error!");
	        return FAIL_RETURN;
	    }

	    //�����������
	    aliyun_iot_mqtt_set_network_param(pClient->ipstack,g_userInfo.hostAddress, g_userInfo.port, g_authInfo->pubkey);
	}

	/* Ignoring return code. failures expected if network is disconnected */
	rc = aliyun_iot_mqtt_connect(pClient, &(pClient->connectdata));

	//���֤����֤������������ȡ֤�飨���洢֤���ļ���
	if(ERROR_CERT_VERIFY_FAIL == rc)
	{
        //ֻ������ȡ֤��
        if(0 != (rc = pull_server_param(g_authInfo->signDataType,CERT_SERVER_PARAM,g_authInfo)))
        {
            WRITE_IOT_ERROR_LOG("cert verify failed,run pull_server_param() error!");
            return rc;
        }

        //��֤֤��Ϸ���
        if(SUCCESS_RETURN != aliyun_iot_verify_certificate(g_authInfo,CERT_SERVER_PARAM,g_authInfo->signDataType))
        {
            WRITE_IOT_ERROR_LOG("cert verify failed,run aliyun_iot_verify_certificate() error!");
            return ERROR_CERT_VERIFY_FAIL;
        }
	}

	if(SUCCESS_RETURN != rc)
	{
	    WRITE_IOT_ERROR_LOG("run aliyun_iot_mqtt_connect() error!");
	    return rc;
	}

	return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_handle_reconnect
* ��       ��: mqtt�ֶ������ӿ�
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: �û��ֶ������ӿڣ����Զ�����ʧ�ܻ���û�������Զ�����ʱʹ�ô˽ӿڽ�������
************************************************************/
int aliyun_iot_mqtt_handle_reconnect(Client *pClient)
{
	int rc = FAIL_RETURN;
	IOT_FUNC_ENTRY;

	if(NULL == pClient)
    {
        return NULL_VALUE_ERROR;
    }

	if(!expired(&(pClient->reconnectparams.reconnectDelayTimer)))
	{
        /* Timer has not expired. Not time to attempt reconnect yet. Return attempting reconnect */
		return FAIL_RETURN;
    }
	
	WRITE_IOT_INFO_LOG("start reconnect");

	
    rc = aliyun_iot_mqtt_attempt_reconnect(pClient);
    if(SUCCESS_RETURN == rc)
    {
        aliyun_iot_mqtt_set_client_state(pClient, CLIENT_STATE_CONNECTED);
        return SUCCESS_RETURN;
    }

    if(ERROR_DEVICE_NOT_EXSIT == rc)
    {
        //�豸ɾ���쳣���������������Ϊ����ֵ
        pClient->reconnectparams.currentReconnectWaitInterval = ALI_IOT_MQTT_DEVICE_ABNORMAL_RECONNECT_WAIT_INTERVAL;
    }
    else
    {
        /*if reconnect network failed, then increase currentReconnectWaitInterval,
        ex: init currentReconnectWaitInterval=1s,  reconnect failed then 2s .4s. 8s*/
        if(ALI_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL > pClient->reconnectparams.currentReconnectWaitInterval)
        {
            pClient->reconnectparams.currentReconnectWaitInterval *= 2;
        }
        else
        {
            pClient->reconnectparams.currentReconnectWaitInterval = ALI_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL;
        }
    }
    countdown_ms(&(pClient->reconnectparams.reconnectDelayTimer),pClient->reconnectparams.currentReconnectWaitInterval);

    pClient->reconnectparams.reconnectNum++;
    WRITE_IOT_ERROR_LOG("mqtt reconnect failed rc = %d", rc);

    return rc;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_release
* ��       ��: mqtt�ͷ�
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: �ͷ�mqtt��ʼ��ʱ��������Դ
************************************************************/
int aliyun_iot_mqtt_release(Client *pClient)
{
    IOT_FUNC_ENTRY;

    if(NULL == pClient)
    {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    aliyun_iot_delete_thread(pClient);
    aliyun_iot_pthread_taskdelay(100);

	aliyun_iot_mqtt_disconnect(pClient);
	aliyun_iot_mqtt_set_client_state(pClient, CLIENT_STATE_INVALID);
	aliyun_iot_pthread_taskdelay(100);

    aliyun_iot_mutex_destory(&pClient->idLock);
    aliyun_iot_mutex_destory(&pClient->subInfoLock);
    aliyun_iot_mutex_destory(&pClient->pubInfoLock);
    aliyun_iot_mutex_destory(&pClient->stateLock);
    aliyun_iot_mutex_destory(&pClient->pingMarkLock);
    aliyun_iot_mutex_destory(&pClient->writebufLock);
    networkRecoverSignalRelease(&pClient->networkRecoverSignal);
    (void)aliyun_iot_sem_destory(&pClient->semaphore);

    list_destroy(pClient->pubInfoList);
    list_destroy(pClient->subInfoList);

    if(NULL != pClient->ipstack)
    {
        aliyun_iot_memory_free(pClient->ipstack);
    }

    WRITE_IOT_NOTICE_LOG("mqtt release!");
    IOT_FUNC_EXIT_RC(SUCCESS_RETURN);
}

/***********************************************************
* ��������: aliyun_iot_mqtt_disconnect
* ��       ��: mqttЭ��Ͽ�����
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: mqttЭ���disconnect�������disconnect
************************************************************/
int aliyun_iot_mqtt_disconnect(Client *pClient)
{
    if(NULL == pClient)
    {
        IOT_FUNC_EXIT_RC(NULL_VALUE_ERROR);
    }

    if (!whether_mqtt_client_state_normal(pClient))
    {
        return SUCCESS_RETURN;
    }

	IOT_FUNC_ENTRY;

	(void)MQTTDisconnect(pClient);

	/*close tcp/ip socket or free tls resources*/
	pClient->ipstack->disconnect(pClient->ipstack);

    aliyun_iot_mqtt_set_client_state(pClient, CLIENT_STATE_INITIALIZED);

	WRITE_IOT_NOTICE_LOG("mqtt disconnect!");
	return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_disconnect_callback
* ��       ��: mqtt�Ͽ����ӻص�����
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
void aliyun_iot_mqtt_disconnect_callback(Client *pClient)
{
	IOT_FUNC_ENTRY;

	/*handle callback function*/
    if(NULL != pClient->reconnectparams.disconnectHandler)
	{
        pClient->reconnectparams.disconnectHandler(pClient,pClient->reconnectparams.disconnectHandlerData);
    }
}

/***********************************************************
* ��������: aliyun_iot_mqtt_reconnect_callback
* ��       ��: mqtt�ָ����ӻص�����
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
void aliyun_iot_mqtt_reconnect_callback(Client *pClient)
{
    IOT_FUNC_ENTRY;

    /*handle callback function*/
    if(NULL != pClient->reconnectparams.reconnectHandler)
    {
        pClient->reconnectparams.reconnectHandler(pClient,pClient->reconnectparams.reconnectHandlerData);
    }
}

/***********************************************************
* ��������: aliyun_iot_mqtt_keep_alive
* ��       ��: mqtt���ͱ�������
* �������: Client *pClient
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
int aliyun_iot_mqtt_keep_alive(Client *pClient)
{
	int rc = SUCCESS_RETURN;
	static int ii = 0;
	IOT_FUNC_ENTRY;

	

    if(NULL == pClient)
	{
		return NULL_VALUE_ERROR;
    }

    /*if in disabled state, without having to send ping packets*/
    if(!whether_mqtt_client_state_normal(pClient))
	{
		return SUCCESS_RETURN;
	}
	
	/*if there is no ping_timer timeout, then return success*/
    if(!expired(&pClient->ping_timer))
	{
        return SUCCESS_RETURN;
    }

    //����Ѿ����͵�ping�ź�û��pong��Ӧ�����ظ����ͣ�ֱ���˳�
    aliyun_iot_mutex_lock(&pClient->pingMarkLock);
    if(0 != pClient->pingMark)
    {
        aliyun_iot_mutex_unlock(&pClient->pingMarkLock);
        return SUCCESS_RETURN;
    }
    aliyun_iot_mutex_unlock(&pClient->pingMarkLock);

    //���ʱ��Ҫ��¼��ʱʱ���ж��Ƿ���������������
	rc = MQTTKeepalive(pClient);
    if(SUCCESS_RETURN != rc)
    {
        if(rc == MQTT_NETWORK_ERROR)
        {
            aliyun_iot_mqtt_set_client_state(pClient, CLIENT_STATE_DISCONNECTED);
        }
        WRITE_IOT_ERROR_LOG("ping outstanding is error,result = %d",rc);
        return rc;
    }

    
    WRITE_IOT_INFO_LOG("start to send ping packet, %d",ii++);

    //����ping�ź������ñ�־λ
    aliyun_iot_mutex_lock(&pClient->pingMarkLock);
    pClient->pingMark = 1;
    aliyun_iot_mutex_unlock(&pClient->pingMarkLock);

	return SUCCESS_RETURN;
}

/***********************************************************
* ��������: aliyun_iot_mqtt_yield
* ��       ��: mqtt�ȴ�������
* �������: Client *pClient
*           int timeout_ms
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��:
************************************************************/
void aliyun_iot_mqtt_yield(Client* pClient, int timeout_ms)
{
	
    int rc = SUCCESS_RETURN;
    Timer timer;
	IOT_FUNC_ENTRY;
    InitTimer(&timer);    

//    countdown_ms(&timer, timeout_ms);
    do
    {
        countdown_ms(&timer, timeout_ms);

		/*acquire package in cycle, such as PINGRESP  PUBLISH*/
		rc = cycle(pClient, &timer);
		if(SUCCESS_RETURN != rc)
		{
		    WRITE_IOT_DEBUG_LOG("cycle failure,rc=%d",rc);
			break;
		}
    } while (!expired(&timer));

    if(SUCCESS_RETURN != rc)
    {
        aliyun_iot_pthread_taskdelay(1000);
    }
}

/***********************************************************
* ��������: aliyun_iot_mqtt_network_event_callback
* ��       ��: �����¼��ص�����
* �������: ALIYUN_IOT_NETWORK_E event �����¼�����
*          void *data �Զ������
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: ����ײ��¼��ص�����ͬӲ��ƽ̨��ʵ�ֲ�ͬ����Ҫ�û����ݻ���ʵ��
************************************************************/
int aliyun_iot_mqtt_network_event_callback(ALIYUN_IOT_NETWORK_E event, void *data)
{
    if(pClientStatus == NULL)
    {
        return NULL_VALUE_ERROR;
    }

	switch(event)
	{
		case ALI_IOT_NETWORK_CONNECTED:
		{	
		    WRITE_IOT_NOTICE_LOG("physical network connected");
			break;
		}

		case ALI_IOT_NETWORK_DISCONNECTED:
		{
		    WRITE_IOT_NOTICE_LOG("physical network disconnected");
			break;
		}

		default:
		    WRITE_IOT_ERROR_LOG("error event,event = %d",event);
		    break;
	}
	
	return SUCCESS_RETURN;
}

