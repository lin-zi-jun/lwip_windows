//#include <unistd.h>
//#include <fcntl.h>
//#include <string.h>
//#include "ucos_ii.h"
#include <errno.h>
#include "aliyun_iot_platform_network.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_platform_memory.h"

#define CANONNAME_MAX 128
//static int errno = 0;
typedef struct NETWORK_ERRNO_TRANS
{
    INT32 systemData;
    ALIYUN_NETWORK_ERROR_E netwokErrno;
    INT32 privateData;
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

INT32 errno_transform(INT32 systemErrno,ALIYUN_NETWORK_ERROR_E *netwokErrno,INT32 *privateErrno)
{
    INT32 num = sizeof(g_networkErrnoTrans);
    INT32 i = 0;
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

INT32 aliyun_iot_get_errno(void)
{
    ALIYUN_NETWORK_ERROR_E networkErrno = NETWORK_FAIL;
    INT32 priv = 0;
    INT32 result = errno_transform(errno,&networkErrno,&priv);
    if(0 != result)
    {
        WRITE_IOT_ERROR_LOG("network errno = %d",errno);
        return NETWORK_FAIL;
    }

    return priv;
}

INT32 aliyun_iot_network_send(INT32 sockFd, void *buf, INT32 nbytes, UINT32 flags)
{
    UINT32 flag = 0;

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

INT32 aliyun_iot_network_recv(INT32 sockFd, void *buf, INT32 nbytes, UINT32 flags)
{
    UINT32 flag = 0;

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
        flag = 0;
    }

    return recv(sockFd,buf,nbytes,flag);
}

INT32 aliyun_iot_network_select(INT32 fd,IOT_NET_TRANS_TYPE_E type,int timeoutMs,IOT_NET_FD_ISSET_E* result)
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
        if( fd < 0 )
        {
            return NETWORK_FAIL;
        }

        if (0 != FD_ISSET(fd, &sets))
        {
            *result = IOT_NET_FD_ISSET;
        }
    }

    return rc;
}

INT32 aliyun_iot_network_settimeout(INT32 fd,int timeoutMs,IOT_NET_TRANS_TYPE_E type)
{
    struct timeval timeout = {timeoutMs/1000, (timeoutMs%1000)*1000};

    int optname = type == IOT_NET_TRANS_RECV ? SO_RCVTIMEO:SO_SNDTIMEO;

    if( fd < 0 )
    {
        return NETWORK_FAIL;
    }

    if(0 != setsockopt(fd, SOL_SOCKET, optname, (char *)&timeout, sizeof(timeout)))
    {
        WRITE_IOT_ERROR_LOG("setsockopt error, errno = %d",errno);
        return ERROR_NET_SETOPT_TIMEOUT;
    }

    return SUCCESS_RETURN;
}

INT32 aliyun_iot_network_get_nonblock(INT32 fd)
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

INT32 aliyun_iot_network_set_nonblock(INT32 fd)
{
	INT32 flags = 0;
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

INT32 aliyun_iot_network_set_block(INT32 fd)
{
	INT32 flags = 0;
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

INT32 aliyun_iot_network_close(INT32 fd)
{
    return close(fd);
}

INT32 aliyun_iot_network_shutdown(INT32 fd,INT32 how)
{
    return shutdown(fd,how);
}

INT32 aliyun_iot_network_create(const INT8*host,const INT8*service,IOT_NET_PROTOCOL_TYPE type)
{
    struct addrinfo hints;
    struct addrinfo *addrInfoList = NULL;
    struct addrinfo *cur = NULL;
    struct sockaddr_in serv_addr;
    int fd = 0;
    int rc = ERROR_NET_UNKNOWN_HOST;

    memset( &hints, 0, sizeof(hints));

    //
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
    
    //fd = (int) socket( cur->ai_family, cur->ai_socktype,cur->ai_protocol );
//    fd = socket(AF_INET, SOCK_STREAM, 0);
//    memset(&serv_addr, 0, sizeof(serv_addr));
//   serv_addr.sin_len = sizeof(serv_addr);
//    serv_addr.sin_family = AF_INET;
//    serv_addr.sin_port = PP_HTONS(80);
//    serv_addr.sin_addr.s_addr = inet_addr("106.11.62.15");
//    //serv_addr.sin_addr.s_addr = inet_addr("121.42.188.97");
//    printf("[ZZZZ]  connect before\n");
//    if (connect(fd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
//    {
//       printf("[ZZZZ]  connect error\n");
//        return -1;
//    }
//    printf("[ZZZZ]  connect OK\n");
//     rc = fd;
//     return rc;    
    printf("[ZZZZ]  getaddrinfot\n");

    if ((rc = getaddrinfo( host, service, &hints, &addrInfoList ))!= 0 )
    {
        WRITE_IOT_ERROR_LOG("getaddrinfo error! rc = %d, errno = %d",rc,errno);
        return ERROR_NET_UNKNOWN_HOST;
    }

    for( cur = addrInfoList; cur != NULL; cur = cur->ai_next )
    {
        //
        if (cur->ai_family != AF_INET)
        {
            WRITE_IOT_ERROR_LOG("socket type error");
            rc = ERROR_NET_SOCKET;
            continue;
        }
        printf("[ZZZZ]  socket before\n");
        fd = (int) socket( cur->ai_family, cur->ai_socktype,cur->ai_protocol );
        if( fd < 0 )
        {
            WRITE_IOT_ERROR_LOG("create socket error,fd = %d, errno = %d",fd,errno);
            rc = ERROR_NET_SOCKET;
            continue;
        }
        printf("[ZZZZ]  connect before\n");
        if( connect( fd,cur->ai_addr,cur->ai_addrlen ) == 0 )
        {
            rc = fd;
            break;
        }
        printf("[ZZZZ]  connect error\n");
        close( fd );
        WRITE_IOT_ERROR_LOG("connect error,errno = %d",errno);
        rc = ERROR_NET_CONNECT;
    }

    freeaddrinfo(addrInfoList);
    printf("[ZZZZ]  connect OK\n");
    return rc;
}

INT32 aliyun_iot_network_bind(const INT8*host,const INT8*service,IOT_NET_PROTOCOL_TYPE type)
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
        //hints.ai_flags = AI_PASSIVE;
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

