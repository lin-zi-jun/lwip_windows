//#include <errno.h>

#include "aliyun_iot_platform_network.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_platform_memory.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/tcp.h"
#include "lwip/err.h"

#define CANONNAME_MAX 128

typedef struct NETWORK_ERRNO_TRANS
{
    int32_t systemData;
    ALIYUN_NETWORK_ERROR_E netwokErrno;
    int32_t privateData;
}NETWORK_ERRNO_TRANS_S;

static NETWORK_ERRNO_TRANS_S g_networkErrnoTrans[]=
{
    {EINTR,NETWORK_SIGNAL_INTERRUPT,EINTR_IOT},
    {EBADF,NETWORK_BAD_FILEFD,EBADF_IOT},
    {EAGAIN,NETWORK_TRYAGAIN,EAGAIN_IOT},
    {EFAULT,NETWORK_BADADDRESS,EFAULT_IOT},
    {EBUSY,NETWORK_RESOURCE_BUSY, EBUSY_IOT},
    {EINVAL,NETWORK_INVALID_ARGUMENT, EINVAL_IOT},
    {ENFILE,NETWORK_FILE_TABLE_OVERFLOW, ENFILE_IOT},
    {EMFILE,NETWORK_TOO_MANY_OPEN_FILES, EMFILE_IOT},
    {ENOSPC,NETWORK_NO_SPACE_LEFT_ON_DEVICE, ENOSPC_IOT},
    {EPIPE,NETWORK_BROKEN_PIPE, EPIPE_IOT},
    {EWOULDBLOCK,NETWORK_OPERATION_BLOCK, EWOULDBLOCK_IOT},
    {ENOTSOCK,NETWORK_OPERATION_ON_NONSOCKET, ENOTSOCK_IOT},
    {ENOPROTOOPT,NETWORK_PROTOCOL_NOT_AVAILABLE, ENOPROTOOPT_IOT},
    {EADDRINUSE,NETWORK_ADDRESS_ALREADY_IN_USE, EADDRINUSE_IOT},
    {EADDRNOTAVAIL,NETWORK_CANNOT_ASSIGN_REQUESTED_ADDRESS, EADDRNOTAVAIL_IOT},
    {ENETDOWN,NETWORK_NETWORK_IS_DOWN, ENETDOWN_IOT},
    {ENETUNREACH,NETWORK_NETWORK_IS_UNREACHABLE, ENETUNREACH_IOT},
    {ENETRESET,NETWORK_CONNECT_RESET, ENETRESET_IOT},
    {ECONNRESET,NETWORK_CONNECT_RESET_BY_PEER, ECONNRESET_IOT},
    {ENOBUFS,NETWORK_NO_BUFFER_SPACE, ENOBUFS_IOT},
    {EISCONN,NETWORK_ALREADY_CONNECTED, EISCONN_IOT},
    {ENOTCONN,NETWORK_IS_NOT_CONNECTED, ENOTCONN_IOT},
    {ETIMEDOUT,NETWORK_CONNECTION_TIMED_OUT, ETIMEDOUT_IOT},
    {ECONNREFUSED,NETWORK_CONNECTION_REFUSED, ECONNREFUSED_IOT},
    {EHOSTDOWN,NETWORK_HOST_IS_DOWN, EHOSTDOWN_IOT},
    {EHOSTUNREACH,NETWORK_NO_ROUTE_TO_HOST, EHOSTUNREACH_IOT},
    {ENOMEM ,NETWORK_OUT_OF_MEMORY, ENOMEM_IOT},
    {EMSGSIZE,NETWORK_MSG_TOO_LONG, EMSGSIZE_IOT}
};

/***********************************************************
* 函数名称: errno_transform
* 描       述: errno的转换
* 输入参数: int32_t systemErrno 系统的errno
*           ALIYUN_NETWORK_ERROR_E *netwokErrno 错误类型
*           int32_t *privateErrno 转换后的错误编码
* 输出参数:
* 返 回  值: 自定义errno
* 说       明: 将linux系统下的errno转换为sdk自定义的errno
************************************************************/
static int32_t errno_transform(int32_t systemErrno,ALIYUN_NETWORK_ERROR_E *netwokErrno,int32_t *privateErrno)
{
    int32_t num = sizeof(g_networkErrnoTrans);
    int32_t i = 0;
    for(i = 0;i<num;i++)
    {
        if(g_networkErrnoTrans[i].systemData == systemErrno)
        {
            *netwokErrno = g_networkErrnoTrans[i].netwokErrno;
            *privateErrno = g_networkErrnoTrans[i].privateData;
            return NETWORK_SUCCESS;
        }
    }

    return NETWORK_FAIL;
}

/***********************************************************
* 函数名称: aliyun_iot_get_errno
* 描       述: 获取SDK定义的errno
* 输入参数:
* 输出参数:
* 返 回  值: 自定义errno
* 说       明: 将linux系统下的errno转换为sdk自定义的errno
************************************************************/
int32_t aliyun_iot_get_errno(void)
{
    ALIYUN_NETWORK_ERROR_E networkErrno = NETWORK_FAIL;
    int32_t private = 0;
    int32_t result = errno_transform(errno,&networkErrno,&private);
    if(0 != result)
    {
        ALIOT_LOG_ERROR("network errno = %d",errno);
        return NETWORK_FAIL;
    }

    return private;
}

/***********************************************************
* 函数名称: aliyun_iot_network_send
* 描       述: 发送网络数据接口
* 输入参数: int32_t sockFd socket描述符
*           void *buf 数据缓存
*           int32_t nbytes 缓存区大小
*           IOT_NET_TRANS_FLAGS_E flags 操作标志
* 输出参数:
* 返 回  值: 同linux系统下标准socket返回值
* 说       明: 网络数据发送，linux系统下的实现
*           源码和mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_send(int32_t sockFd, void *buf, int32_t nbytes, IOT_NET_TRANS_FLAGS_E flags)
{
    uint32_t flag = 0;

    if( sockFd < 0 )
    {
        return NETWORK_FAIL;
    }

    if(IOT_NET_FLAGS_DEFAULT == flags)
    {
        flag = 0;
    }

    return send(sockFd,buf,nbytes,flag);
}

/***********************************************************
* 函数名称: aliyun_iot_network_recv
* 描       述: 接收网络数据接口
* 输入参数: int32_t sockFd socket描述符
*           void *buf 数据缓存
*           int32_t nbytes 缓存区大小
*           IOT_NET_TRANS_FLAGS_E flags 操作标志
* 输出参数:
* 返 回  值: 同linux系统下标准socket返回值
* 说       明: 网络数据发送，linux系统下的实现
*           源码和mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_recv(int32_t sockFd, void *buf, int32_t nbytes, IOT_NET_TRANS_FLAGS_E flags)
{
    uint32_t flag = 0;

    if( sockFd < 0 )
    {
        return NETWORK_FAIL;
    }

    if(IOT_NET_FLAGS_DEFAULT == flags)
    {
        flag = 0;
    }
    else
    {
        flag = MSG_DONTWAIT;
    }

    return recv(sockFd,buf,nbytes,flag);
}

/***********************************************************
* 函数名称: aliyun_iot_network_select
* 描       述: 有可读可写事件判断接口
* 输入参数: int32_t fd socket描述符
*           IOT_NET_TRANS_TYPE_E type 判断读写事件类型
*           int timeoutMs 超时时间
* 输出参数: IOT_NET_FD_ISSET_E* result 可读可写事件结果
* 返 回  值: 同linux系统下标准select的返回值
* 说       明: 判断socket是否有可读写事件，使用linux系统下select接口进行实现
*           源码和mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_select(int32_t fd,IOT_NET_TRANS_TYPE_E type,int timeoutMs,IOT_NET_FD_ISSET_E* result)
{
    struct timeval *timePointer = NULL;
    fd_set *rd_set = NULL;
    fd_set *wr_set = NULL;
    fd_set *ep_set = NULL;
    int rc = 0;
    fd_set sets;

	struct timeval timeout = {timeoutMs/1000, (timeoutMs%1000)*1000};
    *result = IOT_NET_FD_NO_ISSET;

    if( fd < 0 )
    {
        return NETWORK_FAIL;
    }

    FD_ZERO(&sets);
    FD_SET(fd, &sets);

    if(IOT_NET_TRANS_RECV == type)
    {
        rd_set = &sets;
    }
    else
    {
        wr_set = &sets;
    }

    if(0 != timeoutMs)
    {
        timePointer = &timeout;
    }
    else
    {
        timePointer = NULL;
    }

    rc = select(fd+1,rd_set,wr_set,ep_set,timePointer);
    if(rc > 0)
    {
        if (0 != FD_ISSET(fd, &sets))
        {
            *result = IOT_NET_FD_ISSET;
        }
    }

    return rc;
}

/***********************************************************
* 函数名称: aliyun_iot_network_settimeout
* 描       述: 设置socketopt的超时参数
* 输入参数: int32_t fd socket描述符
*           int timeoutMs 超时时间
*           IOT_NET_TRANS_TYPE_E type 读写类型
* 输出参数:
* 返 回  值: 0：成功 非零：失败
* 说       明: linux系统下通过setsocketopt接口设置收发数据的超时时间
*           源码和mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_settimeout(int32_t fd,int timeoutMs,IOT_NET_TRANS_TYPE_E type)
{
    struct timeval timeout = {timeoutMs/1000, (timeoutMs%1000)*1000};

    int optname = type == IOT_NET_TRANS_RECV ? SO_RCVTIMEO:SO_SNDTIMEO;

    if( fd < 0 )
    {
        return NETWORK_FAIL;
    }
    printf("[ZZZZ] type[%d]\n", type);
    if(0 != setsockopt(fd, SOL_SOCKET, optname, (char *)&timeout, sizeof(timeout)))
    {
        ALIOT_LOG_ERROR("setsockopt error, errno = %d",errno);
        return ERROR_NET_SETOPT_TIMEOUT;
    }

    return SUCCESS_RETURN;
}

/***********************************************************
* 函数名称: aliyun_iot_network_get_nonblock
* 描       述: 获取socket非阻塞状态
* 输入参数: int32_t fd socket描述符
* 输出参数:
* 返 回  值: 0：阻塞  非零：非阻塞
* 说       明: linux系统下通过fcntl接口获取当前socket是否是非阻塞接口
*           mbedtls会使用此接口
*           mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_get_nonblock(int32_t fd)
{
    if( fd < 0 )
    {
        return NETWORK_FAIL;
    }

    if( ( fcntl( fd, F_GETFL, 0 ) & O_NONBLOCK ) != O_NONBLOCK )
    {
        return 0;
    }

    if(errno == EAGAIN || errno == EWOULDBLOCK)
    {
        return 1;
    }

    return 0 ;
}

/***********************************************************
* 函数名称: aliyun_iot_network_set_nonblock
* 描       述: 设置socket非阻塞状态
* 输入参数: int32_t fd socket描述符
* 输出参数:
* 返 回  值: 0：成功  非零：失败
* 说       明: linux系统下通过fcntl接口设置当前socket为非阻塞接口
*           mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_set_nonblock(int32_t fd)
{
	int32_t flags = 0;
    if( fd < 0 )
    {
        return NETWORK_FAIL;
    }

    flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, (flags | O_NONBLOCK)) < 0)
    {
        return NETWORK_FAIL;
    }

    return NETWORK_SUCCESS;
}

/***********************************************************
* 函数名称: aliyun_iot_network_set_block
* 描       述: 设置socket阻塞状态
* 输入参数: int32_t fd socket描述符
* 输出参数:
* 返 回  值: 0：成功  非零：失败
* 说       明: linux系统下通过fcntl接口设置当前socket为阻塞接口
*           mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_set_block(int32_t fd)
{
	int32_t flags = 0;
    if( fd < 0 )
    {
        return NETWORK_FAIL;
    }

    flags = fcntl(fd, F_GETFL, 0);
    if (fcntl(fd, F_SETFL, (flags & (~O_NONBLOCK))) < 0)
    {
        return NETWORK_FAIL;
    }

    return NETWORK_SUCCESS;
}

/***********************************************************
* 函数名称: aliyun_iot_network_close
* 描       述: 网络socket关闭
* 输入参数: int32_t fd socket描述符
* 输出参数:
* 返 回  值: 0：成功  非零：失败
* 说       明: linux系统socket关闭
*           mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_close(int32_t fd)
{
    return close(fd);
}

/***********************************************************
* 函数名称: aliyun_iot_network_shutdown
* 描       述: 网络socket的shutdown
* 输入参数: int32_t fd socket描述符
* 输出参数:
* 返 回  值: 0：成功  非零：失败
* 说       明: linux系统socket的shutdown
*           mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_shutdown(int32_t fd,int32_t how)
{
    return shutdown(fd,how);
}

/***********************************************************
* 函数名称: aliyun_iot_network_create
* 描       述: 客户端网络资源创建、连接
* 输入参数: int8_t*host 域名或主机IP
*           int8_t*service 端口或服务名
*           IOT_NET_PROTOCOL_TYPE type 协议类型
* 输出参数:
* 返 回  值:  0：成功，非零：失败
* 说       明: 使用系统接口创建socket，获取域名对应主机IP，
*           并且建立TCP连接
*           源码以及mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_create(const int8_t*host,const int8_t*service,IOT_NET_PROTOCOL_TYPE type)
{
    struct addrinfo hints;
    struct addrinfo *addrInfoList = NULL;
    struct addrinfo *cur = NULL;
    int fd = 0;
    int rc = ERROR_NET_UNKNOWN_HOST;

    memset( &hints, 0, sizeof(hints));

    //默认支持IPv4的服务
    if(IOT_NET_PROTOCOL_TCP == type)
    {
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_protocol = IPPROTO_TCP;
    }
    else
    {
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_protocol = IPPROTO_UDP;
    }

    printf("[ZZZZ] before getaddrinfo\n");
    if ((rc = getaddrinfo( host, service, &hints, &addrInfoList ))!= 0 )
    {
        ALIOT_LOG_ERROR("getaddrinfo error! rc = %d, errno = %d",rc,errno);
        return ERROR_NET_UNKNOWN_HOST;
    }
    printf("[ZZZZ] before getaddrinfo\n");
    
    for( cur = addrInfoList; cur != NULL; cur = cur->ai_next )
    {
        //默认只支持IPv4
        if (cur->ai_family != AF_INET)
        {
            ALIOT_LOG_ERROR("socket type error");
            rc = ERROR_NET_SOCKET;
            continue;
        }

        fd = (int) socket( cur->ai_family, cur->ai_socktype,cur->ai_protocol );
        if( fd < 0 )
        {
            ALIOT_LOG_ERROR("create socket error,fd = %d, errno = %d",fd,errno);
            rc = ERROR_NET_SOCKET;
            continue;
        }

        if( connect( fd,cur->ai_addr,cur->ai_addrlen ) == 0 )
        {
            rc = fd;
            break;
        }

        close( fd );
        ALIOT_LOG_ERROR("connect error,errno = %d",errno);
        rc = ERROR_NET_CONNECT;
    }

    freeaddrinfo(addrInfoList);

    return rc;
}

/***********************************************************
* 函数名称: aliyun_iot_network_bind
* 描       述: 服务端网络资源创建、连接
* 输入参数: int8_t*host 域名或主机IP
*           int8_t*service 端口或服务名
*           IOT_NET_PROTOCOL_TYPE type 协议类型
* 输出参数:
* 返 回  值:  0：成功，非零：失败
* 说       明: 使用系统接口创建socket，获取域名对应主机IP，
*           并且建立TCP连接
*           mbedtls中使用
************************************************************/
int32_t aliyun_iot_network_bind(const int8_t*host,const int8_t*service,IOT_NET_PROTOCOL_TYPE type)
{
    int fd = 0;
    int n = 0;
    int ret = FAIL_RETURN;
    struct addrinfo hints, *addrList, *cur;

    /* Bind to IPv6 and/or IPv4, but only in the desired protocol */
    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET;
    hints.ai_socktype = type == IOT_NET_PROTOCOL_UDP ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_protocol = type == IOT_NET_PROTOCOL_UDP ? IPPROTO_UDP : IPPROTO_TCP;
    if( host == NULL )
    {
        hints.ai_flags = AI_PASSIVE;
    }

    if( getaddrinfo( host, service, &hints, &addrList ) != 0 )
    {
        return( ERROR_NET_UNKNOWN_HOST );
    }

    for( cur = addrList; cur != NULL; cur = cur->ai_next )
    {
        fd = (int) socket( cur->ai_family, cur->ai_socktype,cur->ai_protocol );
        if( fd < 0 )
        {
            ret = ERROR_NET_SOCKET;
            continue;
        }

        n = 1;
        if( setsockopt( fd, SOL_SOCKET, SO_REUSEADDR,(const char *) &n, sizeof( n ) ) != 0 )
        {
            close(fd);
            ret = ERROR_NET_SOCKET;
            continue;
        }

        if( bind(fd, cur->ai_addr, cur->ai_addrlen ) != 0 )
        {
            close( fd );
            ret = ERROR_NET_BIND;
            continue;
        }

        /* Listen only makes sense for TCP */
        if(type == IOT_NET_PROTOCOL_TCP)
        {
            if( listen( fd, 10 ) != 0 )
            {
                close( fd );
                ret = ERROR_NET_LISTEN;
                continue;
            }
        }

        /* I we ever get there, it's a success */
        ret = fd;
        break;
    }

    freeaddrinfo( addrList );

    return( ret );
}

