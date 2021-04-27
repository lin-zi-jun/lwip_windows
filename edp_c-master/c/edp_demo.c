//#include <stdio.h>
//#include <stdlib.h>
//@#include <unistd.h>
#include <string.h>
//@#include <sys/types.h>
//@#include <sys/socket.h>
//@#include <netinet/in.h>
//@#include <netdb.h> 

#include "edp_demo.h"
#include "EdpKit.h"

#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include <stdio.h>
/*---------------------------------------------------------------------------*/
/* Error Code                                                                */
/*---------------------------------------------------------------------------*/
#define ERR_CREATE_SOCKET   -1 
#define ERR_HOSTBYNAME      -2 
#define ERR_CONNECT         -3 
#define ERR_SEND            -4
#define ERR_TIMEOUT         -5
#define ERR_RECV            -6
/*---------------------------------------------------------------------------*/
/* Socket Function                                                           */
/*---------------------------------------------------------------------------*/
#ifndef htonll
#ifdef _BIG_ENDIAN
#define htonll(x)   (x)
#define ntohll(x)   (x)
#else
//@#define htonll(x)   ((((uint64)htonl(x)) << 32) + htonl(x >> 32))
//@#define ntohll(x)   ((((uint64)ntohl(x)) << 32) + ntohl(x >> 32))
#define htonll(x)   (x)
#define ntohll(x)   (x)

#endif
#endif

#if LWIP_SOCKET&&LWIP_ONENET_APP
#define Socket(a,b,c)          socket(a,b,c)
#define Connect(a,b,c)         connect(a,b,c)
#define Close(a)               close(a)
#define Read(a,b,c)            read(a,b,c)
#define Recv(a,b,c,d)          recv(a, (void *)b, c, d)
#define Select(a,b,c,d,e)      select(a,b,c,d,e)
#define Send(a,b,c,d)          send(a, (const int8 *)b, c, d)
#define Write(a,b,c)           write(a,b,c)
#define GetSockopt(a,b,c,d,e)  getsockopt((int)a,(int)b,(int)c,(void *)d,(socklen_t *)e)
#define SetSockopt(a,b,c,d,e)  setsockopt((int)a,(int)b,(int)c,(const void *)d,(int)e)
#define GetHostByName(a)       gethostbyname((const char *)a)

#define RELAYA_ID "relaya"     //
#define RELAYB_ID "relayb"     //
#define IMAGE_ID   "image"     //
#define TIME_ID   "sys_time"
//#define DEV_ID "76633"                         //change to your device-ID
//#define API_KEY "oIMo6LR2GC58vYkM8qbnGxOWzgkA" //change to your API-Key
#define DEV_ID "7558895"
#define API_KEY "2cFWuEAf6lg0tX4mvw=gCYVfMO0="

#define SERVER_ADDR "jjfaedp.hedevice.com"    //OneNet EDP server addr
//#define SERVER_ADDR "api.heclouds.com"
//#define SERVER_ADDR "183.230.40.39"
#define SERVER_PORT 876                       //OneNet EDP server port

typedef struct onenet_ctl
{
    char * str;
    int16 value;
    int16 change;
}onenet_ctl_t;

onenet_ctl_t myctl[5]= {{RELAYA_ID, 0, 1},
                                      {RELAYB_ID, 0, 1}};

typedef struct onenet_upload
{
    u32_t image_peroid;
    u32_t image_tick;
    u32_t time_peroid;
    u32_t time_tick;
}onenet_upload_t;

onenet_upload_t my_upload = {10000, 0, 5000, 0}; //upload image every 6s, upload time every 4s

int32 Open(const uint8 *addr, int16 portno)
{
    int32 sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    /* create socket */
    sockfd = Socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
	    printf("Socket alloc ERROR\n");
        return ERR_CREATE_SOCKET; 
    }
    server = GetHostByName(addr);
    if (server == NULL) {
        Close(sockfd);
	 printf("DNS ERROR\n");
        return ERR_HOSTBYNAME;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_len = sizeof(serv_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = PP_HTONS(portno);
    //addr.sin_addr.s_addr = inet_addr(SOCK_TARGET_HOST);
    serv_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)server->h_addr_list[0])));
 
    if (Connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    {
        printf("ERROR connecting\n");
        Close(sockfd);
        return ERR_CONNECT;
    }

    return sockfd;
}

int32 DoSend(int32 sockfd, const char* buffer, uint32 len)
{
    uint32 total  = 0;
    int32 n = 0;
    while (len != total)
    {
        n = Send(sockfd,buffer + total,len - total,0);
        if (n <= 0)
        {
            //fprintf(stderr, "ERROR writing to socket\n");
            return n;
        }
        total += n;
    }
    return total;
}
int16 get_number(char * str)
{
    int16 ret = 0;
    int i = 0;
    if(str == NULL) return -1;
    while(*str == ' ' || *str == ':') str++;
	
    while(str[i] >= '0' && str[i] <= '9')
    {
        ret = ret*10+(str[i] - '0');
        i++;
        if(i > 5) break;
    }
	
    if(i == 0) return -1;
	
    return ret;
}
int16 update_stream(char *str, int16 num)
{
    int i = 0;
    if(str == NULL) return -1;
	
    while(*str == ' ' || *str == '"') str++;
    
    for(i = 0; i < 5; i++)
    {
        if(strncmp(str, myctl[i].str, 6) == 0)
        {
            if(myctl[i].value != num)
            {
                myctl[i].value = num;
                myctl[i].change = 1;
            }
    			
            return 0;
        }
    }

    return -1;
}
void update_ctl(void)
{
	if(myctl[0].change)
	{
            printf("led0 change to [%d]\n", myctl[0].value);
		
	}
	if(myctl[1].change)
	{
	    printf("led1 change to [%d]\n", myctl[1].value);
	}
}
char *get_picture(int *len)
{
    FILE * fp = NULL;
    int n = 0;
    char * ret = NULL;
    static int i = 1;
    char name[50] = {0,};
    sprintf(name, "pics/image%d.jpg", i);
    i++;
    if(i == 11) i = 1;

    if((fp=fopen(name,"rb"))== NULL)
    {
        printf("The file %s can not be opened.\n",name);
        return NULL;
    }
    do
    {
        fseek(fp,0L,SEEK_END);  
        n = ftell(fp);
        if( n < 0 || n > 20*1024)
        {
            printf("The file [%s] too large.\n", name);
            break;
        }

        fseek(fp,0L,SEEK_SET);
		
        ret = (char *)malloc(n);
        if(ret == NULL)
        {
            printf("malloc [%d] fail.\n", n);
            break;
        }

        if(fread(ret, 1, n, fp) <= 0)
        {
            printf("fread fail.\n");
            free(ret);
            break;
        }
        *len = n;
        printf("[image]fread [%s] return len[%d].\n", name, n);
        fclose(fp);
        return ret;

	}while(0);

	fclose(fp);
	return NULL;
}

static char buffer[512];
static char cmd_buf[50];
static char req_buf[50];
int recv_func(int arg)
{
    int sockfd = arg;
    int error = 0;
    int n, rtn;
    uint8 mtype, jsonorbin;
    
    RecvBuffer* recv_buf = NewBuffer();
    EdpPacket* pkg = NULL;
    EdpPacket* pkg_send = NULL;
    
    char* src_devid;
    char* push_data;
    uint32 push_datalen;

    cJSON* save_json;
    char* save_json_str;

    cJSON* desc_json;
    char* desc_json_str;
    char* save_bin; 
    uint32 save_binlen;
    int ret = 0;

    char *cmdid = NULL;
    uint16 cmdid_len = 0;
    char* req = NULL;
    uint32 req_len = 0;
    int16 num = -1;
    do
    {
        n = Recv(sockfd, buffer, 512, 0);
        if (n <= 0)
        {
            printf("recv error, bytes: %d\n", n);
			error = -1;
            break;
        }
		
        //printf("recv from server, bytes: %d\n", n);
		
        WriteBytes(recv_buf, buffer, n);
        while (1)
        {   
            if ((pkg = GetEdpPacket(recv_buf)) == 0)
            {
                //printf("need more bytes...\n");
                break;
            }
		
            mtype = EdpPacketType(pkg);
            switch(mtype)
            {
                case CONNRESP:
                    rtn = UnpackConnectResp(pkg);
                    printf("recv connect resp, rtn: %d\n", rtn);
                    break;
                case PUSHDATA:
                    UnpackPushdata(pkg, &src_devid, &push_data, &push_datalen);
                    printf("recv push data, src_devid: %s, push_data: %s, len: %d\n", 
                            src_devid, push_data, push_datalen);
                    free(src_devid);
                    free(push_data);
                    break;
                case SAVEDATA:
                    if (UnpackSavedata(pkg, &src_devid, &jsonorbin) == 0)
                    {
                        if (jsonorbin == 0x01) 
                        {/* json */
                            ret = UnpackSavedataJson(pkg, &save_json);
                            save_json_str=cJSON_Print(save_json);
                            printf("recv save data json, ret = %d, src_devid: %s, json: %s\n", 
                                   ret, src_devid, save_json_str);
                            free(save_json_str);
                            cJSON_Delete(save_json);
                        }
                        else if (jsonorbin == 0x02)
                        {/* bin */
                            UnpackSavedataBin(pkg, &desc_json, (uint8**)&save_bin, &save_binlen);
                            desc_json_str=cJSON_Print(desc_json);
                            printf("recv save data bin, src_devid: %s, desc json: %s, bin: %s, binlen: %d\n", 
                                    src_devid, desc_json_str, save_bin, save_binlen);
                            free(desc_json_str);
                            cJSON_Delete(desc_json);
                            free(save_bin);
                        }
                        free(src_devid);
                    }
                    break;
                case PINGRESP:
                    UnpackPingResp(pkg); 
                    printf("recv ping resp\n");
                    break;
                case CMDREQ:
                    if(UnpackCmdReq(pkg, &cmdid, &cmdid_len, &req, &req_len) != 0)
                    {
                        break;
                    }
                    memset(cmd_buf, 0, 50);
                    memset(req_buf, 0, 50);
                    memcpy(cmd_buf, cmdid, cmdid_len);
                    memcpy(req_buf, req, req_len);
                    printf("[Cmd]  CMDREQ cmd[%s]cmdlen[%d], req[%s]reqlen[%d]\n", \
                                                               cmd_buf, cmdid_len, req_buf, req_len);
                    num = get_number(req+7);
                    if(num < 0)
                    {
                        free(cmdid);
                        free(req);
                        break;
                    }

		     update_stream(req, num);
		     update_ctl();
             
                    pkg_send = PacketCmdResp(cmdid, cmdid_len, req, req_len);
                    ret = DoSend(sockfd, (const char *)pkg_send->_data, pkg_send->_write_pos);
	            printf("[Cmd]  PacketCmdResp DoSend: ret = %d\n", ret);
                    DeleteBuffer(&pkg_send);
                    free(cmdid);
                    free(req);
                    break;
                default:
                    printf("unknown type...\n");
                    break;
            }
            DeleteBuffer(&pkg);
        }
    }while(0);
	
    DeleteBuffer(&recv_buf);

    return error;
}

int write_func(int arg)
{
    int sockfd = arg;
    int image_len = 0;
    char *image_data = NULL;
    EdpPacket* send_pkg = NULL;
    cJSON *desc_json;
    int32 ret = 0, flag = 0;
    uint32_t data = 0;
    //char text[25] = {0};
    char text_bin[]="{\"ds_id\": \"image\"}";
    int i = 0;

    //(1) upload led status
    for(i = 0; i < 2; i++)
    {
	  if(myctl[i].change)
	  {
		send_pkg = PacketSavedataInt(kTypeFullJson, NULL, myctl[i].str, (int)myctl[i].value, 0, NULL);
		if(NULL == send_pkg)
		{
			printf("[%s] PacketSavedataInt error\n", myctl[i].str);
			return -1;
		}
	
		ret = DoSend(sockfd, send_pkg->_data, send_pkg->_write_pos);
		printf("[%s]DoSend: [%d], ret = %d\n", myctl[i].str, myctl[i].value, ret);
		DeleteBuffer(&send_pkg);
		send_pkg = NULL;
			
		flag++;
		myctl[i].change = 0;
	  }
    }

    //(1) send current alarm status to onenet. 
    if(sys_now()- my_upload.time_tick >= my_upload.time_peroid)
    {
	send_pkg = PacketSavedataInt(kTypeFullJson, NULL, TIME_ID, (int)sys_now()/1000, 0, NULL);
	if(NULL == send_pkg)
	{
		printf("[time] PacketSavedataInt error\n");
		return -1;
	}

	ret = DoSend(sockfd, (const char *)send_pkg->_data, send_pkg->_write_pos);
	printf("[time] DoSend: ret = %d\n", data, ret);
	DeleteBuffer(&send_pkg);
	send_pkg = NULL;

       flag++;
	my_upload.time_tick = sys_now();
    }
    //(2) try to send a picture to onenet
    if(sys_now()- my_upload.image_tick >= my_upload.image_peroid)
    {	
	image_data = get_picture(&image_len);
	if(image_data == NULL)
	{
	    my_upload.image_tick = sys_now();
            printf("[image]get_picture error\n");
	    return 0;
	}
        desc_json=cJSON_Parse(text_bin);
        send_pkg = PacketSavedataBin(NULL, desc_json, (const uint8*)image_data, image_len);

	free(image_data);
	cJSON_Delete(desc_json);
	if(NULL == send_pkg)
	{
		printf("[image] PacketSavedataBin error\n");
		return -1;
	}

	ret = DoSend(sockfd, (const char *)send_pkg->_data, send_pkg->_write_pos);
	printf("[image]write_func DoSend: ret = %d\n", ret);
        DeleteBuffer(&send_pkg);
        
	flag++;
       my_upload.image_tick = sys_now();
    }

    if(flag == 0)
    {
        sys_msleep(200);
    }
    
    return ret;
}

int client_process_func(void* arg)
{
    int sockfd = *(int*)arg;
    fd_set readset;
    fd_set writeset;
    int i, maxfdp1;
	
    for (;;)
    {
        maxfdp1 = sockfd+1;
	
        /* Determine what sockets need to be in readset */
        FD_ZERO(&readset);
        FD_ZERO(&writeset);
        FD_SET(sockfd, &readset);
        FD_SET(sockfd, &writeset);

        i = select(maxfdp1, &readset, &writeset, 0, 0);
        
        if (i == 0)
            continue;
		
        if (FD_ISSET(sockfd, &readset))
        {
            /* This socket is ready for reading*/
            if (recv_func(sockfd) < 0)
                break;
        }
        /* This socket is ready for writing*/
        if (FD_ISSET(sockfd, &writeset))
        {
            if (write_func(sockfd) < 0)
                break;
        }
		
    }

    return -1;
}
void edp_demo(void * arg)
{
    int sockfd, ret;
	
    EdpPacket* send_pkg;

    arg = arg;
	
    while(1)
    {
	    /* create a socket and connect to server */
        sockfd = Open((const uint8 *)SERVER_ADDR, SERVER_PORT);
        if (sockfd < 0)
        {
            printf("server open error\n");
            sys_msleep(5000);
            continue;
        }
    
        /* connect to server */
        send_pkg = PacketConnect1(DEV_ID, API_KEY);

        printf("send connect to server, bytes: %d\n", send_pkg->_write_pos);
        ret=DoSend(sockfd, (const char *)send_pkg->_data, send_pkg->_write_pos);
        DeleteBuffer(&send_pkg);

        client_process_func((void*)&sockfd);

		/*recv error happen, just close socket*/
        Close(sockfd);
        printf("socket error, close socket and try again\n");

	/*
        c = getchar();
        if (c == '0')
        {
            send_pkg = PacketPing(); 
            printf("send ping to server, bytes: %d\n", send_pkg->_write_pos);
            DoSend(sockfd, send_pkg->_data, send_pkg->_write_pos);
            DeleteBuffer(&send_pkg);
        }
        else if (c == '1')
        {
#ifdef _DEV1
            send_pkg = PacketPushdata("45523", push_data, sizeof(push_data)); 
#else
            send_pkg = PacketPushdata("25267", push_data, sizeof(push_data)); 
#endif
            printf("send pushdata to server, bytes: %d\n", send_pkg->_write_pos);
            DoSend(sockfd, send_pkg->_data, send_pkg->_write_pos);
            DeleteBuffer(&send_pkg);
        }
        else if (c == '2')
        {
            save_json=cJSON_Parse(text1);
#ifdef _DEV1
            send_pkg = PacketSavedataJson("45523", save_json); 
#else
            send_pkg = PacketSavedataJson("25267", save_json); 
#endif
            cJSON_Delete(save_json);
            printf("send savedata json to server, bytes: %d\n", send_pkg->_write_pos);
            DoSend(sockfd, send_pkg->_data, send_pkg->_write_pos);
            DeleteBuffer(&send_pkg);
        }
        else if (c == '3')
        {
            desc_json=cJSON_Parse(text2);
#ifdef _DEV1
            send_pkg = PacketSavedataBin("45523", desc_json, save_bin, sizeof(save_bin)); 
#else
            send_pkg = PacketSavedataBin("25267", desc_json, save_bin, sizeof(save_bin)); 
#endif
            cJSON_Delete(desc_json);
            printf("send savedata bin to server, bytes: %d\n", send_pkg->_write_pos);
            DoSend(sockfd, send_pkg->_data, send_pkg->_write_pos);
            DeleteBuffer(&send_pkg);
        }

        */
    }
    
    //return 0;
}

void edpdemo_init(void)
{
    sys_thread_new("edp_demo", edp_demo, 0, 512, TCPIP_THREAD_PRIO + 1);
}

#endif

