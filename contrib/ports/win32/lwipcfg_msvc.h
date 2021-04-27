/**
 * Additional settings for the win32 port.
 * Copy this to lwipcfg_msvc.h and make the config changes you need.
 */

/* configuration for this port */
#define PPP_USERNAME  "Admin"
#define PPP_PASSWORD  "pass"

/** Define this to the index of the windows network adapter to use */
//#define PACKET_LIB_ADAPTER_NR         1
/** Define this to the GUID of the windows network adapter to use
 * or NOT define this if you want PACKET_LIB_ADAPTER_NR to be used */ 

//ÌîÐ´µçÄÔÍø¿¨
#define PACKET_LIB_ADAPTER_GUID       "4A1371B8-1F3E-4AAB-AD30-D69A54F6234D"
//#define PACKET_LIB_ADAPTER_GUID       "0D90D418-F860-4B19-AF34-2C7618B9477F"
/*#define PACKET_LIB_GET_ADAPTER_NETADDRESS(addr) IP4_ADDR((addr), 192,168,1,0)*/
/*#define PACKET_LIB_QUIET*/

/* If these 2 are not defined, the corresponding config setting is used */
/* #define USE_DHCP    0 */
/* #define USE_AUTOIP  0 */

/* #define USE_PCAPIF 1 */
//ÌîÐ´Íø¹ØÐÅÏ¢
#define LWIP_PORT_INIT_IPADDR(addr)   IP4_ADDR((addr), 192,168,0,109)
#define LWIP_PORT_INIT_GW(addr)       IP4_ADDR((addr), 192,168,0,1)
#define LWIP_PORT_INIT_NETMASK(addr)  IP4_ADDR((addr), 255,255,255,0)

/* remember to change this MAC address to suit your needs!
   the last octet will be increased by netif->num for each netif */
//#define LWIP_MAC_ADDR_BASE            {0x4C,0xBB,0x58,0x74,0x2F,0x7D} //real MAC:7D
//ÌîÐ´MAC
#define LWIP_MAC_ADDR_BASE            {0x3C,0x9C,0x0F,0xB6,0x75,0x8D} //real MAC:7D


/* #define USE_SLIPIF 0 */
/* #define SIO_USE_COMPORT 0 */
#ifdef USE_SLIPIF
#if USE_SLIPIF
#define LWIP_PORT_INIT_SLIP1_IPADDR(addr)   IP4_ADDR((addr), 192, 168,   2, 2)
#define LWIP_PORT_INIT_SLIP1_GW(addr)       IP4_ADDR((addr), 192, 168,   2, 1)
#define LWIP_PORT_INIT_SLIP1_NETMASK(addr)  IP4_ADDR((addr), 255, 255, 255, 0)
#if USE_SLIPIF > 1
#define LWIP_PORT_INIT_SLIP2_IPADDR(addr)   IP4_ADDR((addr), 192, 168,   2, 1)
#define LWIP_PORT_INIT_SLIP2_GW(addr)       IP4_ADDR((addr), 0,     0,   0, 0)
#define LWIP_PORT_INIT_SLIP2_NETMASK(addr)  IP4_ADDR((addr), 255, 255, 255, 0)*/
#endif /* USE_SLIPIF > 1 */
#endif /* USE_SLIPIF */
#endif /* USE_SLIPIF */

/* configuration for applications */

#define LWIP_CHARGEN_APP              0
#define LWIP_DNS_APP                  1
#define LWIP_HTTPD_APP                0
/* Set this to 1 to use the netconn http server,
 * otherwise the raw api server will be used. */
/*#define LWIP_HTTPD_APP_NETCONN     */
#define LWIP_NETBIOS_APP              0
#define LWIP_NETIO_APP                0
#define LWIP_PING_APP                 0
#define LWIP_RTP_APP                  0
#define LWIP_SHELL_APP                0
#define LWIP_SNTP_APP                 0
#define LWIP_SOCKET_EXAMPLES_APP      0
#define LWIP_TCPECHO_APP              0
/* Set this to 1 to use the netconn tcpecho server,
 * otherwise the raw api server will be used. */
/*#define LWIP_TCPECHO_APP_NETCONN   */
#define LWIP_UDPECHO_APP              0
#define LWIP_LWIPERF_APP              0
#define LWIP_ONENET_APP                1
//some configurations for HTTPD

/*#define USE_AUTOIP  1*/
#if LWIP_DNS_APP
#define DNS_SERVER_ADDRESS(ipaddr) IP4_ADDR(ipaddr, 208, 67, 222, 222);
//#define DNS_SERVER_ADDRESS(ipaddr) (ip4_addr_set_u32(ipaddr, ipaddr_addr("208.67.222.222")))
#endif    //resolver1.opendns.com
/* define this to your custom application-init function */
/* #define LWIP_APP_INIT my_app_init() */
