#ifndef __COMMON_H__
#define __COMMON_H__

/*---------------------------------------------------------------------------*/
/* Type Definition Macros                                                    */
/*---------------------------------------------------------------------------*/
#ifndef __WORDSIZE
  /* Assume 32 */
  #define __WORDSIZE 32
#endif
#include "lwip/arch.h"
#if defined(_LINUX) || defined (WIN32)
    typedef u8_t   uint8;
    typedef s8_t   int8;
    typedef u16_t  uint16;
    typedef s16_t  int16;
    typedef u32_t  uint32;
    typedef s32_t  int32;
#endif

#ifdef WIN32
    typedef int socklen_t;
#endif

#if defined(WIN32)
    //typedef unsigned long long int  uint64;
    //typedef long long int           int64;
#elif (__WORDSIZE == 32)
    __extension__
    typedef long long int           int64;
    __extension__
    typedef unsigned long long int  uint64;
#elif (__WORDSIZE == 64)
    typedef unsigned long int       uint64;
    typedef long int                int64;
#endif

#endif /* __COMMON_H__ */
