#ifndef ALIYUN_IOT_COMMON_UTIL_H
#define ALIYUN_IOT_COMMON_UTIL_H

typedef enum ALIYUN_IOT_TOPIC_TYPE
{
    TOPIC_NAME_TYPE = 0,
    TOPIC_FILTER_TYPE
}ALIYUN_IOT_TOPIC_TYPE_E;

/***********************************************************
* ��������: aliyun_iot_common_check_topic
* ��       ��: topicУ��
* �������: const char * topicName
*          ALIYUN_IOT_TOPIC_TYPE_E type У������
* �������: VOID
* �� ��  ֵ: 0���ɹ�  ��0��ʧ��
* ˵       ��: topicnameУ��ʱ��������+��#����
*           topicfilterУ��ʱ+��#������ڵ������ǵ�����һ��·����Ԫ��
*           ��#ֻ�ܴ��������һ��·����Ԫ
************************************************************/
int aliyun_iot_common_check_topic(const char * topicName,ALIYUN_IOT_TOPIC_TYPE_E type);

#endif
