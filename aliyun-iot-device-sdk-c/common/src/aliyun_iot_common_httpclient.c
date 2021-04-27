/* Copyright (C) 2012 mbed.org, MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 * BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#include <string.h>
#include "aliyun_iot_platform_network.h"
#include "aliyun_iot_platform_stdio.h"
#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"
#include "aliyun_iot_common_log.h"
#include "aliyun_iot_common_httpclient.h"
#pragma warning(disable:4100 4018 4127 4047)
#define HTTPCLIENT_MIN(x,y) (((x)<(y))?(x):(y))
#define HTTPCLIENT_MAX(x,y) (((x)>(y))?(x):(y))

#define HTTPCLIENT_AUTHB_SIZE     128

#define HTTPCLIENT_CHUNK_SIZE     512
#define HTTPCLIENT_SEND_BUF_SIZE  512

#define HTTPCLIENT_MAX_HOST_LEN   64
#define HTTPCLIENT_MAX_URL_LEN    512

#if defined(MBEDTLS_DEBUG_C)
#define DEBUG_LEVEL 2
#endif

static int httpclient_parse_host(char *url, char *host, UINT32 maxhost_len);
static int httpclient_parse_url(const char *url, char *scheme, UINT32 max_scheme_len, char *host, UINT32 maxhost_len, int *port, char *path, UINT32 max_path_len);
static int httpclient_tcp_send_all(int sock_fd, char *data, int length);
static int httpclient_conn(httpclient_t *client, char *host, int port);
static int httpclient_recv(httpclient_t *client, char *buf, int min_len, int max_len, int *p_read_len);
static int httpclient_retrieve_content(httpclient_t *client, char *data, int len, httpclient_data_t *client_data);
static int httpclient_response_parse(httpclient_t *client, char *data, int len, httpclient_data_t *client_data);

static void httpclient_base64enc(char *out, const char *in)
{
    const char code[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
    int i = 0, x = 0, l = 0;

    for (; *in; in++)
    {
        x = x << 8 | *in;
        for (l += 8; l >= 6; l -= 6)
        {
            out[i++] = code[(x >> (l - 6)) & 0x3f];
        }
    }
    if (l > 0)
    {
        x <<= 6 - l;
        out[i++] = code[x & 0x3f];
    }
    for (; i % 4;)
    {
        out[i++] = '=';
    }
    out[i] = '\0';
}

int httpclient_conn(httpclient_t *client, char *url, int port)
{
    char host[HTTPCLIENT_MAX_HOST_LEN];

    memset(host,0,HTTPCLIENT_MAX_HOST_LEN);
    httpclient_parse_host(url, host, sizeof(host));
    //memcpy(host, "106.11.62.15", strlen("106.11.62.15"));
    WRITE_IOT_DEBUG_LOG("host:%s", host);
    printf("[ZZZZ]  aliyun_iot_network_create before host[%s]\n", host);
    client->socket = aliyun_iot_network_create(host,"80",IOT_NET_PROTOCOL_TCP);
    if(client->socket < 0)
    {
        WRITE_IOT_ERROR_LOG("httpclient_conn error");
        return client->socket;
    }

    return SUCCESS_RETURN;
}

int httpclient_parse_url(const char *url, char *scheme, UINT32 max_scheme_len, char *host, UINT32 maxhost_len, int *port, char *path, UINT32 max_path_len)
{
    char *scheme_ptr = (char *) url;
    char *host_ptr = (char *) strstr(url, "://");
    UINT32 host_len = 0;
    UINT32 path_len;
    char *port_ptr;
    char *path_ptr;
    char *fragment_ptr;

    if (host_ptr == NULL)
    {
        WRITE_IOT_ERROR_LOG("Could not find host");
        return ERROR_HTTP_PARSE; /* URL is invalid */
    }

    if (max_scheme_len < host_ptr - scheme_ptr + 1)
    { /* including NULL-terminating char */
        WRITE_IOT_ERROR_LOG("Scheme str is too small (%u >= %u)", max_scheme_len, (UINT32)(host_ptr - scheme_ptr + 1));
        return ERROR_HTTP_PARSE;
    }
    memcpy(scheme, scheme_ptr, host_ptr - scheme_ptr);
    scheme[host_ptr - scheme_ptr] = '\0';

    host_ptr += 3;

    port_ptr = strchr(host_ptr, ':');
    if (port_ptr != NULL)
    {
        UINT16 tport;
        host_len = port_ptr - host_ptr;
        port_ptr++;
        if (sscanf(port_ptr, "%hu", &tport) != 1)
        {
            WRITE_IOT_ERROR_LOG("Could not find port");
            return ERROR_HTTP_PARSE;
        }
        *port = (int) tport;
    }
    else
    {
        *port = 0;
    }
    path_ptr = strchr(host_ptr, '/');
    if (host_len == 0)
    {
        host_len = path_ptr - host_ptr;
    }

    if (maxhost_len < host_len + 1)
    { /* including NULL-terminating char */
        WRITE_IOT_ERROR_LOG("Host str is too small (%d >= %d)", maxhost_len, host_len + 1);
        return ERROR_HTTP_PARSE;
    }
    memcpy(host, host_ptr, host_len);
    host[host_len] = '\0';

    fragment_ptr = strchr(host_ptr, '#');
    if (fragment_ptr != NULL)
    {
        path_len = fragment_ptr - path_ptr;
    }
    else
    {
        path_len = strlen(path_ptr);
    }

    if (max_path_len < path_len + 1)
    { /* including NULL-terminating char */
        WRITE_IOT_ERROR_LOG("Path str is too small (%d >= %d)", max_path_len, path_len + 1);
        return ERROR_HTTP_PARSE;
    }
    memcpy(path, path_ptr, path_len);
    path[path_len] = '\0';

    return SUCCESS_RETURN;
}

int httpclient_parse_host(char *url, char *host, UINT32 maxhost_len)
{
    char *host_ptr = (char *) strstr(url, "://");
    UINT32 host_len = 0;
    char *port_ptr;
    char *path_ptr;

    if (host_ptr == NULL)
    {
        WRITE_IOT_ERROR_LOG("Could not find host");
        return ERROR_HTTP_PARSE; /* URL is invalid */
    }
    host_ptr += 3;

    port_ptr = strchr(host_ptr, ':');
    if (port_ptr != NULL)
    {
        UINT16 tport;
        host_len = port_ptr - host_ptr;
        port_ptr++;
        if (sscanf(port_ptr, "%hu", &tport) != 1)
        {
            WRITE_IOT_ERROR_LOG("Could not find port");
            return ERROR_HTTP_PARSE;
        }
    }

    path_ptr = strchr(host_ptr, '/');
    if (host_len == 0)
    {
        host_len = path_ptr - host_ptr;
    }

    if (maxhost_len < host_len + 1)
    { /* including NULL-terminating char */
        WRITE_IOT_ERROR_LOG("Host str is too small (%d >= %d)", maxhost_len, host_len + 1);
        return ERROR_HTTP_PARSE;
    }
    memcpy(host, host_ptr, host_len);
    host[host_len] = '\0';

    return SUCCESS_RETURN;
}

int httpclient_get_info(httpclient_t *client, char *send_buf, int *send_idx, char *buf, UINT32 len) /* 0 on success, err code on failure */
{
    int ret;
    int cp_len;
    int idx = *send_idx;

    if (len == 0)
    {
        len = strlen(buf);
    }

    do
    {
        if ((HTTPCLIENT_SEND_BUF_SIZE - idx) >= len)
        {
            cp_len = len;
        }
        else
        {
            cp_len = HTTPCLIENT_SEND_BUF_SIZE - idx;
        }

        memcpy(send_buf + idx, buf, cp_len);
        idx += cp_len;
        len -= cp_len;

        if (idx == HTTPCLIENT_SEND_BUF_SIZE)
        {
            if (client->remote_port == HTTPS_PORT)
            {
                WRITE_IOT_ERROR_LOG("send buffer overflow");
                return ERROR_HTTP;
            }
            ret = httpclient_tcp_send_all(client->socket, send_buf, HTTPCLIENT_SEND_BUF_SIZE);
            if (ret)
            {
                return (ret);
            }
        }
    } while (len);

    *send_idx = idx;
    return SUCCESS_RETURN;
}

void httpclient_set_custom_header(httpclient_t *client, char *header)
{
    client->header = header;
}

int httpclient_basic_auth(httpclient_t *client, char *user, char *password)
{
    if ((strlen(user) + strlen(password)) >= HTTPCLIENT_AUTHB_SIZE)
    {
        return ERROR_HTTP;
    }
    client->auth_user = user;
    client->auth_password = password;
    return SUCCESS_RETURN;
}

int httpclient_send_auth(httpclient_t *client, char *send_buf, int *send_idx)
{
    char b_auth[(int) ((HTTPCLIENT_AUTHB_SIZE + 3) * 4 / 3 + 1)];
    char base64buff[HTTPCLIENT_AUTHB_SIZE + 3];

    httpclient_get_info(client, send_buf, send_idx, "Authorization: Basic ", 0);
    sprintf(base64buff, "%s:%s", client->auth_user, client->auth_password);
    WRITE_IOT_DEBUG_LOG("bAuth: %s", base64buff) ;
    httpclient_base64enc(b_auth, base64buff);
    b_auth[strlen(b_auth) + 1] = '\0';
    b_auth[strlen(b_auth)] = '\n';
    WRITE_IOT_DEBUG_LOG("b_auth:%s", b_auth) ;
    httpclient_get_info(client, send_buf, send_idx, b_auth, 0);
    return SUCCESS_RETURN;
}

int httpclient_tcp_send_all(int sock_fd, char *data, int length)
{
    int written_len = 0;

    while (written_len < length)
    {
        int ret = aliyun_iot_network_send(sock_fd, data + written_len, length - written_len, IOT_NET_FLAGS_DEFAULT);
        if (ret > 0)
        {
            written_len += ret;
            continue;
        }
        else if (ret == 0)
        {
            return 0;
        }
        else
        {
            WRITE_IOT_ERROR_LOG("run aliyun_iot_network_send error,errno = %d",aliyun_iot_get_errno());
            return -1; /* Connnection error */
        }
    }

    return written_len;
}

int httpclient_send_header(httpclient_t *client, char *url, int method, httpclient_data_t *client_data)
{
    char scheme[8] = { 0 };
    char host[HTTPCLIENT_MAX_HOST_LEN] = { 0 };
    char path[HTTPCLIENT_MAX_URL_LEN] = { 0 };
    int len;
    char send_buf[HTTPCLIENT_SEND_BUF_SIZE] = { 0 };
    char buf[HTTPCLIENT_SEND_BUF_SIZE] = { 0 };
    char *meth = (method == HTTPCLIENT_GET) ? "GET" : (method == HTTPCLIENT_POST) ? "POST" : (method == HTTPCLIENT_PUT) ? "PUT" : (method == HTTPCLIENT_DELETE) ? "DELETE" :
                 (method == HTTPCLIENT_HEAD) ? "HEAD" : "";
    int ret;

    /* First we need to parse the url (http[s]://host[:port][/[path]]) */
    int res = httpclient_parse_url(url, scheme, sizeof(scheme), host, sizeof(host), &(client->remote_port), path, sizeof(path));
    if (res != SUCCESS_RETURN)
    {
        WRITE_IOT_ERROR_LOG("httpclient_parse_url returned %d", res);
        return res;
    }

    if (client->remote_port == 0)
    {
        if (strcmp(scheme, "http") == 0)
        {
            client->remote_port = HTTP_PORT;
        }
        else if (strcmp(scheme, "https") == 0)
        {
            client->remote_port = HTTPS_PORT;
        }
    }

    /* Send request */
    memset(send_buf, 0, HTTPCLIENT_SEND_BUF_SIZE);
    len = 0; /* Reset send buffer */

    aliyun_iot_stdio_snprintf(buf, sizeof(buf), "%s %s HTTP/1.1\r\nHost: %s\r\n", meth, path, host); /* Write request */
    ret = httpclient_get_info(client, send_buf, &len, buf, strlen(buf));
    if (ret)
    {
        WRITE_IOT_ERROR_LOG("Could not write request");
        return ERROR_HTTP_CONN;
    }

    /* Send all headers */
    if (client->auth_user)
    {
        httpclient_send_auth(client, send_buf, &len); /* send out Basic Auth header */
    }

    /* Add user header information */
    if (client->header)
    {
        httpclient_get_info(client, send_buf, &len, (char *) client->header, strlen(client->header));
    }

    if (client_data->post_buf != NULL)
    {
        aliyun_iot_stdio_snprintf(buf, sizeof(buf), "Content-Length: %d\r\n", client_data->post_buf_len);
        httpclient_get_info(client, send_buf, &len, buf, strlen(buf));

        if (client_data->post_content_type != NULL)
        {
            aliyun_iot_stdio_snprintf(buf, sizeof(buf), "Content-Type: %s\r\n", client_data->post_content_type);
            httpclient_get_info(client, send_buf, &len, buf, strlen(buf));
        }
    }

    /* Close headers */
    httpclient_get_info(client, send_buf, &len, "\r\n", 0);

    WRITE_IOT_DEBUG_LOG("Trying to write %d bytes http header:%s", len, send_buf);

    ret = httpclient_tcp_send_all(client->socket, send_buf, len);
    if (ret > 0)
    {
        WRITE_IOT_DEBUG_LOG("Written %d bytes, socket = %d", ret, client->socket);
    }
    else if (ret == 0)
    {
        WRITE_IOT_ERROR_LOG("ret == 0,Connection was closed by server");
        return ERROR_HTTP_CLOSED; /* Connection was closed by server */
    }
    else
    {
        WRITE_IOT_ERROR_LOG("Connection error (send returned %d)", ret);
        return ERROR_HTTP_CONN;
    }

    return SUCCESS_RETURN;
}

int httpclient_send_userdata(httpclient_t *client, httpclient_data_t *client_data)
{
    int ret = 0;

    if (client_data->post_buf && client_data->post_buf_len)
    {
        WRITE_IOT_DEBUG_LOG("client_data->post_buf:%s", client_data->post_buf);
        {
            ret = httpclient_tcp_send_all(client->socket, client_data->post_buf, client_data->post_buf_len);
            if (ret > 0)
            {
                WRITE_IOT_DEBUG_LOG("Written %d bytes", ret);
            }
            else if (ret == 0)
            {
                WRITE_IOT_ERROR_LOG("ret == 0,Connection was closed by server");
                return ERROR_HTTP_CLOSED; /* Connection was closed by server */
            }
            else
            {
                WRITE_IOT_ERROR_LOG("Connection error (send returned %d)", ret);
                return ERROR_HTTP_CONN;
            }
        }
    }

    return SUCCESS_RETURN;
}

int httpclient_recv(httpclient_t *client, char *buf, int min_len, int max_len, int *p_read_len) /* 0 on success, err code on failure */
{
    int ret = 0;
    UINT32 readLen = 0;
    IOT_NET_FD_ISSET_E result = IOT_NET_FD_NO_ISSET;

    while (readLen < max_len)
    {
        buf[readLen] = '\0';
        if (client->remote_port != HTTPS_PORT)
        {
        	ret = aliyun_iot_network_select(client->socket,IOT_NET_TRANS_RECV,500,&result);
	        if (ret < 0)
	        {
	            INT32 err = aliyun_iot_get_errno();
	            if(err == EINTR_IOT)
	            {
	                continue;
	            }
	            else
	            {
	                WRITE_IOT_ERROR_LOG("http read(select) fail ret=%d", ret);
	                return -1;
	            }
	        }
	        else if (ret == 0)
	        {
	            WRITE_IOT_NOTICE_LOG("http read(select) timeout");
	            break;
	        }
	        else if(ret == 1)
	        {
				if(IOT_NET_FD_NO_ISSET == result)
	            {
	                WRITE_IOT_DEBUG_LOG("another fd readable!");
	                continue;
	            }
	        }

	        
            aliyun_iot_network_settimeout(client->socket,50,IOT_NET_TRANS_RECV);

            if (readLen < min_len)
            {
                ret = aliyun_iot_network_recv(client->socket, buf + readLen, min_len - readLen, IOT_NET_FLAGS_DEFAULT);
                WRITE_IOT_DEBUG_LOG("recv [blocking] return:%d", ret);
            }
            else
            {
                ret = aliyun_iot_network_recv(client->socket, buf + readLen, max_len - readLen, IOT_NET_FLAGS_DONTWAIT);
                WRITE_IOT_DEBUG_LOG("recv [not blocking] return:%d", ret);
                if(ret < 0)
                {
                    INT32 err = aliyun_iot_get_errno();
                    if (err == EAGAIN_IOT || err == EWOULDBLOCK_IOT)
                    {
                        WRITE_IOT_DEBUG_LOG("recv [not blocking], ret = %d",ret);
                        break;
                    }
                }
            }
        }

        if (ret > 0)
        {
            readLen += ret;
        }
        else if(ret == 0)
        {
            break;
        }
        else
        {
            WRITE_IOT_ERROR_LOG("Connection error (recv returned %d)", ret);
            *p_read_len = readLen;
            return ERROR_HTTP_CONN;
        }
    }

    WRITE_IOT_DEBUG_LOG("Read %d bytes", readLen);
    *p_read_len = readLen;
    buf[readLen] = '\0';

    return SUCCESS_RETURN;
}

int httpclient_retrieve_content(httpclient_t *client, char *data, int len, httpclient_data_t *client_data)
{
    int count = 0;
    int templen = 0;
    int crlf_pos;
    /* Receive data */
    WRITE_IOT_DEBUG_LOG("Receiving data:%s", data);
    client_data->is_more = TRUE_IOT;

    if (client_data->response_content_len == -1 && client_data->is_chunked == FALSE_IOT)
    {
        while (TRUE_IOT)
        {
            int ret, max_len;
            if (count + len < client_data->response_buf_len - 1)
            {
                memcpy(client_data->response_buf + count, data, len);
                count += len;
                client_data->response_buf[count] = '\0';
            }
            else
            {
                memcpy(client_data->response_buf + count, data, client_data->response_buf_len - 1 - count);
                client_data->response_buf[client_data->response_buf_len - 1] = '\0';
                return HTTP_RETRIEVE_MORE_DATA;
            }

            max_len = HTTPCLIENT_MIN(HTTPCLIENT_CHUNK_SIZE - 1, client_data->response_buf_len - 1 - count);
            ret = httpclient_recv(client, data, 1, max_len, &len);

            /* Receive data */
            WRITE_IOT_DEBUG_LOG("data len: %d %d", len, count);

            if (ret == ERROR_HTTP_CONN)
            {
                WRITE_IOT_DEBUG_LOG("ret == ERROR_HTTP_CONN");
                return ret;
            }

            if (len == 0)
            {/* read no more data */
                WRITE_IOT_DEBUG_LOG("no more len == 0");
                client_data->is_more = FALSE_IOT;
                return SUCCESS_RETURN;
            }
        }
    }

    while (TRUE_IOT)
    {
        UINT32 readLen = 0;

        if (client_data->is_chunked && client_data->retrieve_len <= 0)
        {
            /* Read chunk header */
            BOOL foundCrlf;
            int n;
            do
            {
                foundCrlf = FALSE_IOT;
                crlf_pos = 0;
                data[len] = 0;
                if (len >= 2)
                {
                    for (; crlf_pos < len - 2; crlf_pos++)
                    {
                        if (data[crlf_pos] == '\r' && data[crlf_pos + 1] == '\n')
                        {
                            foundCrlf = TRUE_IOT;
                            break;
                        }
                    }
                }
                if (!foundCrlf)
                { /* Try to read more */
                    if (len < HTTPCLIENT_CHUNK_SIZE)
                    {
                        int new_trf_len, ret;
                        ret = httpclient_recv(client, data + len, 0, HTTPCLIENT_CHUNK_SIZE - len - 1, &new_trf_len);
                        len += new_trf_len;
                        if (ret == ERROR_HTTP_CONN)
                        {
                            return ret;
                        }
                        else
                        {
                            continue;
                        }
                    }
                    else
                    {
                        return ERROR_HTTP;
                    }
                }
            } while (!foundCrlf);
            data[crlf_pos] = '\0';
            n = sscanf(data, "%x", &readLen);/* chunk length */
            client_data->retrieve_len = readLen;
            client_data->response_content_len += client_data->retrieve_len;
            if (n != 1)
            {
                WRITE_IOT_ERROR_LOG("Could not read chunk length");
                return ERRO_HTTP_UNRESOLVED_DNS;
            }

            memmove(data, &data[crlf_pos + 2], len - (crlf_pos + 2)); /* Not need to move NULL-terminating char any more */
            len -= (crlf_pos + 2);

            if (readLen == 0)
            {
                /* Last chunk */
                client_data->is_more = FALSE_IOT;
                WRITE_IOT_DEBUG_LOG("no more (last chunk)");
                break;
            }
        }
        else
        {
            readLen = client_data->retrieve_len;
        }

        WRITE_IOT_DEBUG_LOG("Retrieving %d bytes, len:%d", readLen, len);

        do
        {
            templen = HTTPCLIENT_MIN(len, readLen);
            if (count + templen < client_data->response_buf_len - 1)
            {
                memcpy(client_data->response_buf + count, data, templen);
                count += templen;
                client_data->response_buf[count] = '\0';
                client_data->retrieve_len -= templen;
            }
            else
            {
                memcpy(client_data->response_buf + count, data, client_data->response_buf_len - 1 - count);
                client_data->response_buf[client_data->response_buf_len - 1] = '\0';
                client_data->retrieve_len -= (client_data->response_buf_len - 1 - count);
                return HTTP_RETRIEVE_MORE_DATA;
            }

            if (len > readLen)
            {
                WRITE_IOT_DEBUG_LOG("memmove %d %d %d\n", readLen, len, client_data->retrieve_len);
                memmove(data, &data[readLen], len - readLen); /* chunk case, read between two chunks */
                len -= readLen;
                readLen = 0;
                client_data->retrieve_len = 0;
            }
            else
            {
                readLen -= len;
            }

            if (readLen)
            {
                int ret;
                int max_len = HTTPCLIENT_MIN(HTTPCLIENT_CHUNK_SIZE - 1, client_data->response_buf_len - 1 - count);
                ret = httpclient_recv(client, data, 1, max_len, &len);
                if (ret == ERROR_HTTP_CONN)
                {
                    return ret;
                }
            }
        } while (readLen);

        if (client_data->is_chunked)
        {
            if (len < 2)
            {
                int new_trf_len, ret;
                /* Read missing chars to find end of chunk */
                ret = httpclient_recv(client, data + len, 2 - len, HTTPCLIENT_CHUNK_SIZE - len - 1, &new_trf_len);
                if (ret == ERROR_HTTP_CONN)
                {
                    return ret;
                }
                len += new_trf_len;
            }
            if ((data[0] != '\r') || (data[1] != '\n'))
            {
                WRITE_IOT_ERROR_LOG("Format error, %s", data); /* after memmove, the beginning of next chunk */
                return ERRO_HTTP_UNRESOLVED_DNS;
            }
            memmove(data, &data[2], len - 2); /* remove the \r\n */
            len -= 2;
        }
        else
        {
            WRITE_IOT_DEBUG_LOG("no more(content-length)\n");
            client_data->is_more = FALSE_IOT;
            break;
        }

    }

    return SUCCESS_RETURN;
}

int httpclient_response_parse(httpclient_t *client, char *data, int len, httpclient_data_t *client_data)
{
    int crlf_pos;
	char *crlf_ptr = NULL;
    client_data->response_content_len = -1;

    crlf_ptr = strstr(data, "\r\n");
    if (crlf_ptr == NULL)
    {
        WRITE_IOT_ERROR_LOG("\r\n not found");
        return ERRO_HTTP_UNRESOLVED_DNS;
    }

    crlf_pos = crlf_ptr - data;
    data[crlf_pos] = '\0';

    /* Parse HTTP response */
    if (sscanf(data, "HTTP/%*d.%*d %d %*[^\r\n]", &(client->response_code)) != 1)
    {
        /* Cannot match string, error */
        WRITE_IOT_ERROR_LOG("Not a correct HTTP answer : %s\n", data);
        return ERRO_HTTP_UNRESOLVED_DNS;
    }

    if ((client->response_code < 200) || (client->response_code >= 400))
    {
        /* Did not return a 2xx code; TODO fetch headers/(&data?) anyway and implement a mean of writing/reading headers */
        WRITE_IOT_WARNING_LOG("Response code %d", client->response_code);
    }

    WRITE_IOT_DEBUG_LOG("Reading headers%s", data);

    memmove(data, &data[crlf_pos + 2], len - (crlf_pos + 2) + 1); /* Be sure to move NULL-terminating char as well */
    len -= (crlf_pos + 2);

    client_data->is_chunked = FALSE_IOT;

    /* Now get headers */
    while (TRUE_IOT)
    {
        char key[32];
        char value[32];
        int n;

        key[31] = '\0';
        value[31] = '\0';

        crlf_ptr = strstr(data, "\r\n");
        if (crlf_ptr == NULL)
        {
            if (len < HTTPCLIENT_CHUNK_SIZE - 1)
            {
                int new_trf_len, ret;
                ret = httpclient_recv(client, data + len, 1, HTTPCLIENT_CHUNK_SIZE - len - 1, &new_trf_len);
                len += new_trf_len;
                data[len] = '\0';
                WRITE_IOT_DEBUG_LOG("Read %d chars; In buf: [%s]", new_trf_len, data);
                if (ret == ERROR_HTTP_CONN)
                {
                    return ret;
                }
                else
                {
                    continue;
                }
            }
            else
            {
                WRITE_IOT_DEBUG_LOG("header len > chunksize");
                return ERROR_HTTP;
            }
        }

        crlf_pos = crlf_ptr - data;
        if (crlf_pos == 0)
        { /* End of headers */
            memmove(data, &data[2], len - 2 + 1); /* Be sure to move NULL-terminating char as well */
            len -= 2;
            break;
        }

        data[crlf_pos] = '\0';

        n = sscanf(data, "%31[^:]: %31[^\r\n]", key, value);
        if (n == 2)
        {
            WRITE_IOT_DEBUG_LOG("Read header : %s: %s", key, value);
            if (!strcmp(key, "Content-Length"))
            {
                sscanf(value, "%d", &(client_data->response_content_len));
                client_data->retrieve_len = client_data->response_content_len;
            }
            else if (!strcmp(key, "Transfer-Encoding"))
            {
                if (!strcmp(value, "Chunked") || !strcmp(value, "chunked"))
                {
                    client_data->is_chunked = TRUE_IOT;
                    client_data->response_content_len = 0;
                    client_data->retrieve_len = 0;
                }
            }
            memmove(data, &data[crlf_pos + 2], len - (crlf_pos + 2) + 1); /* Be sure to move NULL-terminating char as well */
            len -= (crlf_pos + 2);

        }
        else
        {
            WRITE_IOT_ERROR_LOG("Could not parse header");
            return ERROR_HTTP;
        }
    }

    return httpclient_retrieve_content(client, data, len, client_data);
}

IOT_RETURN_CODES_E httpclient_connect(httpclient_t *client, char *url, int port)
{
    int ret = ERROR_HTTP_CONN;
    printf("httpclient_connect port[%d]\n", port);
    client->socket = -1;
    if (port != HTTPS_PORT)
    {
        ret = httpclient_conn(client, url, port);
        if (0 == ret)
        {
            client->remote_port = HTTP_PORT;
        }
    }

    return ret;
}

IOT_RETURN_CODES_E httpclient_send_request(httpclient_t *client, char *url, int method, httpclient_data_t *client_data)
{
    int ret = ERROR_HTTP_CONN;

    if (client->socket < 0)
    {
        return ret;
    }

    ret = httpclient_send_header(client, url, method, client_data);
    if (ret != 0)
    {
        WRITE_IOT_ERROR_LOG("httpclient_send_header is error,ret = %d",ret);
        return ret;
    }

    if (method == HTTPCLIENT_POST || method == HTTPCLIENT_PUT)
    {
        ret = httpclient_send_userdata(client, client_data);
    }

    return ret;
}

IOT_RETURN_CODES_E httpclient_recv_response(httpclient_t *client, httpclient_data_t *client_data)
{
    int reclen = 0, ret = ERROR_HTTP_CONN;
    char buf[HTTPCLIENT_CHUNK_SIZE] = { 0 };

    if (client->socket < 0)
    {
        return ret;
    }

    if (client_data->is_more)
    {
        client_data->response_buf[0] = '\0';
        ret = httpclient_retrieve_content(client, buf, reclen, client_data);
    }
    else
    {
        ret = httpclient_recv(client, buf, 1, HTTPCLIENT_CHUNK_SIZE - 1, &reclen);
        if (ret != 0)
        {
            return ret;
        }

        buf[reclen] = '\0';

        if (reclen)
        {
            WRITE_IOT_DEBUG_LOG("reclen:%d, buf:", reclen);
            WRITE_IOT_DEBUG_LOG("%s", buf);
            ret = httpclient_response_parse(client, buf, reclen, client_data);
        }
    }

    return ret;
}

void httpclient_close(httpclient_t *client, int port)
{
    if (port != HTTPS_PORT)
    {
        if (client->socket >= 0)
        {
            aliyun_iot_network_close(client->socket);
        }
        client->socket = -1;
    }
}

int httpclient_common(httpclient_t *client, char *url, int port, int method, httpclient_data_t *client_data)
{
    int ret = ERROR_HTTP_CONN;
    printf("[ZZZZ]  httpclient_connect before\n");
    ret = httpclient_connect(client, url, port);
    if(0 != ret)
    {
        WRITE_IOT_ERROR_LOG("httpclient_connect is error,ret = %d",ret);
        httpclient_close(client, port);
        return ret;
    }
	printf("[ZZZZ]  httpclient_send_request before\n");
    ret = httpclient_send_request(client, url, method, client_data);
    if(0 != ret)
    {
        WRITE_IOT_ERROR_LOG("httpclient_send_request is error,ret = %d",ret);
        httpclient_close(client, port);
        return ret;
    }

    ret = httpclient_recv_response(client, client_data);
    if (0 != ret)
    {
        WRITE_IOT_ERROR_LOG("httpclient_recv_response is error,ret = %d",ret);
        httpclient_close(client, port);
        return ret;
    }

    httpclient_close(client, port);
    return ret;
}

int aliyun_iot_common_get_response_code(httpclient_t *client)
{
    return client->response_code;
}

IOT_RETURN_CODES_E aliyun_iot_common_post(httpclient_t *client, char *url, int port, httpclient_data_t *client_data)
{
    return httpclient_common(client, url, port, HTTPCLIENT_POST, client_data);
}
