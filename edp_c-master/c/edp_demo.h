#ifndef __EDP_DEMO_H__
#define __EDP_DEMO_H__

#include "lwip/opt.h"
#include "lwipcfg_msvc.h"

#if LWIP_SOCKET&&LWIP_ONENET_APP

void edpdemo_init(void);

#endif /* LWIP_SOCKET */


#endif /*__EDP_DEMO_H__*/
