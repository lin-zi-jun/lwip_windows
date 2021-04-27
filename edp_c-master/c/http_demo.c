#include "lwip\sockets.h"
#include "lwip\netif.h"
#include "lwip\dns.h"
#include "lwip\api.h"
#include "lwip\tcp.h"
#include "lwip\netdb.h"
#include "http_demo.h"
#include <stdio.h>
#if LWIP_SOCKET && LWIP_ONENET_APP
/*remember system time*/
static unsigned int sys_timer = 0;
#define YBUFSIZE 512
static char request[YBUFSIZE];
static char send_buf[YBUFSIZE];

#define HTTP_ADDR "api.heclouds.com"
#define HTTP_PORT 80                   //server port

#define DEV_ID "7558895"                                             //change to your device-ID
#define API_KEY "2cFWuEAf6lg0tX4mvw=gCYVfMO0="    //change to your API-Key

#define RELAYA_ID "relaya"     //
#define RELAYB_ID "relayb"     //
int http_open(const char *addr, int portno)
{
    int sockfd;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    int opt;
	
    /* create socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
		printf("yeelink new socket error");
        return -1; 
    }
	
    server = gethostbyname(addr);
    if (server == NULL) {
        close(sockfd);
	printf("yeelink dns error");
        return -1;
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_len = sizeof(serv_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = PP_HTONS(portno);
    //addr.sin_addr.s_addr = inet_addr(SOCK_TARGET_HOST);
    serv_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *)server->h_addr_list[0])));
  
    if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
    {
        printf("yeelink connect error\n");
        close(sockfd);
        return -1;
    }
	
    opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(int)) < 0)
    {
        printf("yeelink setsockopt error\n");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

static void process_recv_html(char * body)
{
	printf("recv html:[%s]\n",body);
}

int http_recv_func(int arg)
{
    int sockfd = arg;
    int count = read(sockfd, request, YBUFSIZE);
    if (count <= 0)
    {
        printf("yeelink conn closed [%d]\n", count);
        return -1;
    }
	
    process_recv_html(request);
    return 0;
}

int send_http_request(int sock)
{
	memset(request,0,YBUFSIZE);
        //get data
        strcat(request,"GET /devices/");
        strcat(request,DEV_ID);
        strcat(request,"/datapoints?datastream_id=");//注意后面必须加上\r\n
        strcat(request,RELAYA_ID);
        strcat(request,",");
        strcat(request,RELAYB_ID);
        strcat(request,"&limit=1 HTTP/1.1\r\n");
        strcat(request,"api-key:");
        strcat(request,API_KEY);
        strcat(request,"\r\n");
        strcat(request,"Host:");
        strcat(request,HTTP_ADDR);
        strcat(request,"\r\n");
        strcat(request,"Connection: Keep-Alive\r\n");
        strcat(request,"Cache-Control: no-cache\r\n\r\n");

       printf("send to server [%s]\n", request);
	return send(sock, request, strlen(request), MSG_DONTWAIT);
}
int send_time_update(int sock)
{
    int sockfd = sock;
    int ret = 0;
    char text[100] = {0};
    char tmp[25] = {0};
  
    /*准备JSON串*/
    strcat(text,"{\"datastreams\":[{");
    strcat(text,"\"id\":\"sys_time\",");
    strcat(text,"\"datapoints\":[");
    strcat(text,"{");
    sprintf(tmp, "\"value\":%d",  sys_now()/1000);
    strcat(text,tmp);
    //strcat(text, "\"value\":50");
    strcat(text,"}]}]}");
  
    /*准备HTTP报头*/
    send_buf[0] = 0;
    strcat(send_buf,"POST /devices/");
    strcat(send_buf,DEV_ID);
    strcat(send_buf,"/datapoints HTTP/1.1\r\n");//注意后面必须加上\r\n
    strcat(send_buf,"api-key:");
    strcat(send_buf,API_KEY);
    strcat(send_buf,"\r\n");
    strcat(send_buf,"Host:");
    strcat(send_buf,HTTP_ADDR);
    strcat(send_buf,"\r\n");
    sprintf(tmp,"Content-Length:%d\r\n\r\n", strlen(text));//计算JSON串长度
    strcat(send_buf,tmp);
    strcat(send_buf,text);
  
    ret = DoSend(sockfd, send_buf, strlen(send_buf));//发送数据
    return ret;  

}
char *get_next_picture(int *len)
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

int send_image_update(int sock)
{
    int sockfd = sock;
    int ret = 0;
    int n = 0;
    //char *text = NULL;
    char tmp[25] = {0};
    
    char *pic = get_next_picture(&n);
    if(pic == NULL) return 0;
    /*准备JSON串*/
    //strcat(text,"{\"datastreams\":[{");
    //strcat(text,"\"id\":\"sys_time\",");
    //strcat(text,"\"datapoints\":[");
    //strcat(text,"{");
    //sprintf(tmp, "\"value\":%d",  sys_now()/1000);
    //strcat(text,tmp);
    ////strcat(text, "\"value\":50");
    //strcat(text,"}]}]}");
  
    /*准备HTTP报头*/
    send_buf[0] = 0;
    strcat(send_buf,"POST /bindata?device_id=");
    strcat(send_buf,DEV_ID);
    strcat(send_buf,"&datastream_id=image HTTP/1.1\r\n");//注意后面必须加上\r\n
    strcat(send_buf,"api-key:");
    strcat(send_buf,API_KEY);
    strcat(send_buf,"\r\n");
    strcat(send_buf,"Host:");
    strcat(send_buf,HTTP_ADDR);
    strcat(send_buf,"\r\n");
    sprintf(tmp,"Content-Length:%d\r\n\r\n", n);//计算JSON串长度
    strcat(send_buf,tmp);
    //strcat(send_buf,text);
  
    ret = DoSend(sockfd, send_buf, strlen(send_buf));//发送数据
    ret = DoSend(sockfd, pic, n);
    return ret;  

}

int http_write_func(int arg)
{
    unsigned int now = sys_now();
    int sockfd = arg;
    static int send_count = 0;

    int ret = 0;
    /*request data every 1 Seconds*/
    if(now - sys_timer < 10000)
    {
	 sys_msleep(500);
        return 0;
    }
    
    send_count ++;
    if(send_count > 5)
    {
        send_count = 0;
        return -1;
    }
        
    sys_timer = now;

    ret = send_http_request(sockfd);
    //ret = send_time_update(sockfd);
    //ret = send_image_update(sockfd);
    return ret;
}

int http_client_process(void* arg)
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
            if (http_recv_func(sockfd) < 0)
                break;
        }
        /* This socket is ready for writing*/
        if (FD_ISSET(sockfd, &writeset))
        {
            if (http_write_func(sockfd) < 0)
                break;
        }
		
    }

    return -1;
}

void http_thread(void * arg)
{
    int sockfd;
		
    sys_timer = sys_now();
    printf("http_thread in\n");
    while(1)
    {
	    /* create a socket and connect to server */
        sockfd = http_open(HTTP_ADDR, HTTP_PORT);
        if (sockfd < 0)
        {
            printf("server open error\n");
            continue;
        }
        
        http_client_process((void*)&sockfd);

        /*recv error happen, just close socket*/
        close(sockfd);
        printf("close and try again\n");
    }
    
}

void init_onenet_http(void)
{
    sys_thread_new("onenet_thread", http_thread, 0, 512, TCPIP_THREAD_PRIO + 2);
    printf("[task]onenet client created\n");
}

#endif


