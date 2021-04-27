#include <stdlib.h>
#include <string.h>
#include "aliyun_iot_common_util.h"
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_common_error.h"

#define TOPINAMEC_LEN_MAX 64

int aliyun_iot_common_check_rule(char *iterm,ALIYUN_IOT_TOPIC_TYPE_E type)
{
	int i = 0;
    int len = 0;
    if(NULL == iterm)
    {
        WRITE_IOT_ERROR_LOG("iterm is NULL");
        return FAIL_RETURN;
    }

    len = strlen(iterm);
    for(i = 0;i<len;i++)
    {
        if(TOPIC_FILTER_TYPE == type)
        {
            if('+' == iterm[i] || '#' == iterm[i])
            {
                if(1 != len)
                {
                    WRITE_IOT_ERROR_LOG("the character # and + is error");
                    return FAIL_RETURN;
                }
            }
        }
        else
        {
            if('+' == iterm[i] || '#' == iterm[i])
            {
                WRITE_IOT_ERROR_LOG("has character # and + is error");
                return FAIL_RETURN;
            }
        }

        if(iterm[i] < 32 || iterm[i] >= 127 )
        {
            return FAIL_RETURN;
        }
    }
    return SUCCESS_RETURN;
}

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
int aliyun_iot_common_check_topic(const char * topicName,ALIYUN_IOT_TOPIC_TYPE_E type)
{
	int mask = 0;
    char topicString[TOPINAMEC_LEN_MAX];
	
    char* delim = "/";
    char* iterm = NULL;

    if(NULL == topicName || '/' != topicName[0])
    {
        return FAIL_RETURN;
    }

    
    memset(topicString,0x0,TOPINAMEC_LEN_MAX);
    strncpy(topicString,topicName,strlen(topicName));

    iterm = strtok(topicString,delim);

    if(SUCCESS_RETURN != aliyun_iot_common_check_rule(iterm,type))
    {
        WRITE_IOT_ERROR_LOG("run aliyun_iot_common_check_rule error");
        return FAIL_RETURN;
    }

    for(;;)
    {
        iterm = strtok(NULL,delim);

        if(iterm == NULL)
        {
            break;
        }

        //��·���а���#�ַ����Ҳ������һ��·����ʱ����
        if(1 == mask)
        {
            WRITE_IOT_ERROR_LOG("the character # is error");
            return FAIL_RETURN;
        }

        if(SUCCESS_RETURN != aliyun_iot_common_check_rule(iterm,type))
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_common_check_rule error");
            return FAIL_RETURN;
        }

        if(iterm[0] == '#')
        {
            mask = 1;
        }
    }

    return SUCCESS_RETURN;
}
