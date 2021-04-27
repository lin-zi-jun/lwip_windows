#ifndef __HTTP_DEMO_H__
#define __HTTP_DEMO_H__

#include "lwip/opt.h"
#include "lwipcfg_msvc.h"

#if LWIP_SOCKET&&LWIP_ONENET_APP

void init_onenet_http(void);

#endif /* LWIP_SOCKET */


#endif /*__EDP_DEMO_H__*/
