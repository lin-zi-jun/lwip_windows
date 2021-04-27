#ifndef ALIYUN_IOT_MQTT_NETTYPE_H
#define ALIYUN_IOT_MQTT_NETTYPE_H

#include "aliyun_iot_network_ssl.h"

typedef struct {
    char *pHostAddress;                 ///< Pointer to a string defining the endpoint for the MQTT service
    char *pHostPort;                    ///< MQTT service listening port
    char *pPubKey;                      ///< Pointer to a string defining the Root CA file (full file, not path)
}ConnectParams;

/**
 * @brief The structure of MQTT network connection used in the MQTT library. The user has to allocate memory for this structure.
 */
typedef struct Network Network;
struct Network
{
    int my_socket;                                                /**< Connect the socket handle. */
    int (*mqttread)(Network *, unsigned char *, int, int);        /**< Read data from server function pointer. */
    int (*mqttwrite)(Network *, unsigned char *, int, int);       /**< Send data to server function pointer. */
    void (*disconnect)(Network *);                                /**< Disconnect the network function pointer.�˺���close socket����Ҫ��ʼ��Ϊ-1�����Ϊ-1����ִ��close����*/
    int (*mqttConnect)(Network *);
    ConnectParams connectparams;
    TLSDataParams tlsdataparams;
};

int aliyun_iot_mqtt_nettype_read(Network *pNetwork, unsigned char *buffer, int len, int timeout_ms);
int aliyun_iot_mqtt_nettype_write(Network *pNetwork, unsigned char *buffer, int len, int timeout_ms);
void aliyun_iot_mqtt_nettype_disconnect(Network *pNetwork);
int aliyun_iot_mqtt_nettype_connect(Network *pNetwork);


#endif
