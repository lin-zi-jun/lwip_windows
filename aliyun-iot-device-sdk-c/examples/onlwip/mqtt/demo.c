#include <stdio.h>

#include "aliyun_iot_mqtt_common.h"
#include "aliyun_iot_mqtt_client.h"
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_auth.h"
#include "aliyun_iot_platform_pthread.h"
#include "aliyun_iot_platform_memory.h"

#define HOST_NAME      "iot-auth.aliyun.com"

//�û���Ҫ�����豸��Ϣ�������º궨���е���Ԫ������
#define PRODUCT_KEY    "IDYJLMyAWTI"
#define PRODUCT_SECRET "daVTsz1cC8nBoTFGqIn5w8RQw2MvT8HW"
#define DEVICE_NAME    "mydev1"
#define DEVICE_SECRET  "daVTsz1cC8nBoTFGqIn5w8RQw2MvT8HW"

//��������TOPIC�ĺ궨�岻��Ҫ�û��޸ģ�����ֱ��ʹ��
//IOT HUBΪ�豸��������TOPIC��update�����豸������Ϣ��error�����豸��������get���ڶ�����Ϣ
#define TOPIC_UPDATE         "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
#define TOPIC_ERROR          "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
#define TOPIC_GET            "/"PRODUCT_KEY"/"DEVICE_NAME"/get"

#define MSG_LEN_MAX 100


/**Number of publish thread*/
#define MAX_PUBLISH_THREAD_COUNT 2


//callback of publish
static void messageArrived(MessageData *md)
{
    char msg[MSG_LEN_MAX] = {0};

    MQTTMessage *message = md->message;
    if(message->payloadlen > MSG_LEN_MAX - 1)
    {
        printf("process part of receive message\n");
        message->payloadlen = MSG_LEN_MAX - 1;
    }

	memcpy(msg,message->payload,message->payloadlen);
	
	printf("Message : %s\n", msg);
}

static void publishComplete(void* context, unsigned int msgId)
{
    printf("publish message is arrived,id = %d\n",msgId);
}

static void subAckTimeout(SUBSCRIBE_INFO_S *subInfo)
{
    printf("msgId = %d,sub ack is timeout\n",subInfo->msgId);
}

void * pubThread(void*param)
{
    char buf[MSG_LEN_MAX] = { 0 };
    static int num = 0;
    int rc = 0 ;
    Client * client = (Client*)param;
    MQTTMessage message;

    int msgId[5] = {0};
	int i = 0;
    static int threadID = 0;
    int id = threadID++;
	

    for(;;)
    {
        for(i = 0; i < 5; i++)
        {
            memset(&message,0x0,sizeof(message));
            sprintf(buf, "{\"message\":\"Hello World! threadId = %d, num = %d\"}",id,num++);
            message.qos = QOS1;
            message.retained = FALSE_IOT;
            message.dup = FALSE_IOT;
            message.payload = (void *) buf;
            message.payloadlen = strlen(buf);
            message.id = 0;
            rc = aliyun_iot_mqtt_publish(client, TOPIC_UPDATE, &message);
            if (0 != rc)
            {
                printf("ali_iot_mqtt_publish failed ret = %d\n", rc);
            }
            else
            {
                msgId[i] = message.id;
            }

            aliyun_iot_pthread_taskdelay(5000);
        }

        aliyun_iot_pthread_taskdelay(1000);
    }

    return NULL;
}

int multiThreadDemo(unsigned char *msg_buf,unsigned char *msg_readbuf)
{
    int rc = 0;
	IOT_DEVICEINFO_SHADOW_S deviceInfo;
	Client client;
	IOT_CLIENT_INIT_PARAMS initParams;
	MQTTPacket_connectData connectParam;
	ALIYUN_IOT_PTHREAD_S publishThread[MAX_PUBLISH_THREAD_COUNT];
	
    INT32 ch;	
	unsigned iter = 0;
	int i = 0;
    memset(msg_buf,0x0,MSG_LEN_MAX);
    memset(msg_readbuf,0x0,MSG_LEN_MAX);

    
    memset(&deviceInfo, 0x0, sizeof(deviceInfo));

    deviceInfo.productKey = PRODUCT_KEY;
    deviceInfo.productSecret = PRODUCT_SECRET;
    deviceInfo.deviceName = DEVICE_NAME;
    deviceInfo.deviceSecret = DEVICE_SECRET;
    deviceInfo.hostName = HOST_NAME;
    if (0 != aliyun_iot_set_device_info(&deviceInfo))
    {
        printf("run aliyun_iot_set_device_info() error!\n");
        return -1;
    }

    if (0 != aliyun_iot_auth(HMAC_MD5_SIGN_TYPE, IOT_VALUE_FALSE))
    {
        printf("run aliyun_iot_auth() error!\n");
        return -1;
    }

    
    memset(&client,0x0,sizeof(client));
    
    memset(&initParams,0x0,sizeof(initParams));

    initParams.mqttCommandTimeout_ms = 2000;
    initParams.pReadBuf = msg_readbuf;
    initParams.readBufSize = MSG_LEN_MAX;
    initParams.pWriteBuf = msg_buf;
    initParams.writeBufSize = MSG_LEN_MAX;
    initParams.disconnectHandler = NULL;
    initParams.disconnectHandlerData = (void*) &client;
    initParams.deliveryCompleteFun = publishComplete;
    initParams.subAckTimeOutFun = subAckTimeout;
    rc = aliyun_iot_mqtt_init(&client, &initParams);
    if (0 != rc)
    {
        printf("ali_iot_mqtt_init failed ret = %d\n", rc);
        return rc;
    }

    memset(&connectParam,0x0,sizeof(connectParam));
    connectParam.cleansession = 1;
    connectParam.MQTTVersion = 4;
    connectParam.keepAliveInterval = 180;
    connectParam.willFlag = 0;
    rc = aliyun_iot_mqtt_connect(&client, &connectParam);
    if (0 != rc)
    {
        printf("ali_iot_mqtt_connect failed ret = %d\n", rc);
        return rc;
    }

    rc = aliyun_iot_mqtt_subscribe(&client, TOPIC_GET, QOS1, messageArrived);
    if (0 != rc)
    {
        printf("ali_iot_mqtt_subscribe failed ret = %d\n", rc);
        return rc;
    }

    do
    {
        aliyun_iot_pthread_taskdelay(100);
        rc = aliyun_iot_mqtt_suback_sync(&client, TOPIC_GET, messageArrived);
    }while(rc != SUCCESS_RETURN);


	for (iter = 0; iter < MAX_PUBLISH_THREAD_COUNT; iter++)
	{
		rc = aliyun_iot_pthread_create(&publishThread[iter], pubThread, &client, NULL);
		if(1 == rc)
		{
			printf("create publish thread success ");
		}
		else
		{
			printf("create publish thread success failed");
		}
	}

    do
    {
        ch = getchar();
        aliyun_iot_pthread_taskdelay(100);
    } while (ch != 'Q' && ch != 'q');


	for (i = 0; i < MAX_PUBLISH_THREAD_COUNT; i++)
	{
		aliyun_iot_pthread_cancel(&publishThread[i]);
	}

    aliyun_iot_mqtt_release(&client);

    return 0;
}

int singleThreadDemo(unsigned char *msg_buf,unsigned char *msg_readbuf)
{
    int rc = 0;
    char buf[MSG_LEN_MAX] = { 0 };
    MQTTMessage message;
    INT32 ch;
    IOT_DEVICEINFO_SHADOW_S deviceInfo;
    MQTTPacket_connectData connectParam;
    Client client;
    IOT_CLIENT_INIT_PARAMS initParams;
    memset(&deviceInfo, 0x0, sizeof(deviceInfo));

    deviceInfo.productKey = PRODUCT_KEY;
    deviceInfo.productSecret = PRODUCT_SECRET;
    deviceInfo.deviceName = DEVICE_NAME;
    deviceInfo.deviceSecret = DEVICE_SECRET;
    deviceInfo.hostName = HOST_NAME;
    printf("run aliyun_iot_set_device_info()!\n");
    if (0 != aliyun_iot_set_device_info(&deviceInfo))
    {
        printf("run aliyun_iot_set_device_info() error!\n");
        return -1;
    }
	printf("run aliyun_iot_auth()!\n");
    if (0 != aliyun_iot_auth(HMAC_MD5_SIGN_TYPE, IOT_VALUE_FALSE))
    {
        printf("run aliyun_iot_auth() error!\n");
        return -1;
    }

    printf("run aliyun_iot_auth OK()!\n");
    memset(&client,0x0,sizeof(client));
    
    memset(&initParams,0x0,sizeof(initParams));

    initParams.mqttCommandTimeout_ms = 2000;
    initParams.pReadBuf = msg_readbuf;
    initParams.readBufSize = MSG_LEN_MAX;
    initParams.pWriteBuf = msg_buf;
    initParams.writeBufSize = MSG_LEN_MAX;
    initParams.disconnectHandler = NULL;
    initParams.disconnectHandlerData = (void*) &client;
    initParams.deliveryCompleteFun = publishComplete;
    initParams.subAckTimeOutFun = subAckTimeout;
    rc = aliyun_iot_mqtt_init(&client, &initParams);
    if (0 != rc)
    {
        printf("aliyun_iot_mqtt_init failed ret = %d\n", rc);
        return rc;
    }
    memset(&connectParam,0x0,sizeof(connectParam));
    connectParam.cleansession = 1;
    connectParam.MQTTVersion = 4;
    connectParam.keepAliveInterval = 180;
    connectParam.willFlag = 0;
	printf("run aliyun_iot_mqtt_connect OK()!\n");
    rc = aliyun_iot_mqtt_connect(&client, &connectParam);
    if (0 != rc)
    {
        aliyun_iot_mqtt_release(&client);
        printf("ali_iot_mqtt_connect failed ret = %d\n", rc);
        return rc;
    }
	printf("run aliyun_iot_mqtt_subscribe OK()!\n");
    rc = aliyun_iot_mqtt_subscribe(&client, TOPIC_GET, QOS1, messageArrived);
    if (0 != rc)
    {
        aliyun_iot_mqtt_release(&client);
        printf("ali_iot_mqtt_subscribe failed ret = %d\n", rc);
        return rc;
    }

    do
    {
        aliyun_iot_pthread_taskdelay(1000);
		printf("run aliyun_iot_mqtt_suback_sync OK()!\n");
        rc = aliyun_iot_mqtt_suback_sync(&client, TOPIC_GET, messageArrived);
    }while(rc != SUCCESS_RETURN);

    
    memset(&message,0x0,sizeof(message));
    sprintf(buf, "{\"message\":\"Hello World\"}");
    message.qos = QOS1;
    message.retained = FALSE_IOT;
    message.dup = FALSE_IOT;
    message.payload = (void *) buf;
    message.payloadlen = strlen(buf);
    message.id = 0;
    rc = aliyun_iot_mqtt_publish(&client, TOPIC_UPDATE, &message);
    if (0 != rc)
    {
        aliyun_iot_mqtt_release(&client);
        printf("ali_iot_mqtt_publish failed ret = %d\n", rc);
        return rc;
    }
	
    do
    {
        ch = getchar();
        aliyun_iot_pthread_taskdelay(100);
    } while (ch != 'Q' && ch != 'q');

    aliyun_iot_mqtt_release(&client);

    aliyun_iot_pthread_taskdelay(10000);
    return 0;
}

int mqtt_client_demo1()
{
    unsigned char *msg_buf = (unsigned char *)aliyun_iot_memory_malloc(MSG_LEN_MAX);
    unsigned char *msg_readbuf = (unsigned char *)aliyun_iot_memory_malloc(MSG_LEN_MAX);
	printf("start demo!\n");
    if (0 != aliyun_iot_auth_init())
    {
        printf("run aliyun_iot_auth_init error!\n");
        return -1;
    }

    singleThreadDemo(msg_buf,msg_readbuf);
    //multiThreadDemo(msg_buf,msg_readbuf);

    aliyun_iot_memory_free(msg_buf);
    aliyun_iot_memory_free(msg_readbuf);

    (void) aliyun_iot_auth_release();

    printf("out of demo!\n");

    return 0;
}

