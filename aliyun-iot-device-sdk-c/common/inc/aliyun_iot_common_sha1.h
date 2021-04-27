#ifndef ALIYUN_IOT_COMMON_SHA1_H
#define ALIYUN_IOT_COMMON_SHA1_H

#include "aliyun_iot_common_datatype.h"
#include "aliyun_iot_common_error.h"


/**
 * \brief          SHA-1 context structure
 */
typedef struct
{
    UINT32 total[2];          /*!< number of bytes processed  */
    UINT32 state[5];          /*!< intermediate digest state  */
    unsigned char buffer[64];   /*!< data block being processed */
}iot_sha1_context;

/**
 * \brief          Initialize SHA-1 context
 *
 * \param ctx      SHA-1 context to be initialized
 */
void aliyun_iot_sha1_init( iot_sha1_context *ctx );

/**
 * \brief          Clear SHA-1 context
 *
 * \param ctx      SHA-1 context to be cleared
 */
void aliyun_iot_sha1_free( iot_sha1_context *ctx );

/**
 * \brief          Clone (the state of) a SHA-1 context
 *
 * \param dst      The destination context
 * \param src      The context to be cloned
 */
void aliyun_iot_sha1_clone( iot_sha1_context *dst,
                         const iot_sha1_context *src );

/**
 * \brief          SHA-1 context setup
 *
 * \param ctx      context to be initialized
 */
void aliyun_iot_sha1_starts( iot_sha1_context *ctx );

/**
 * \brief          SHA-1 process buffer
 *
 * \param ctx      SHA-1 context
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 */
void aliyun_iot_sha1_update( iot_sha1_context *ctx, const unsigned char *input, size_t ilen );

/**
 * \brief          SHA-1 final digest
 *
 * \param ctx      SHA-1 context
 * \param output   SHA-1 checksum result
 */
void aliyun_iot_sha1_finish( iot_sha1_context *ctx, unsigned char output[20] );

/* Internal use */
void aliyun_iot_sha1_process( iot_sha1_context *ctx, const unsigned char data[64] );

/**
 * \brief          Output = SHA-1( input buffer )
 *
 * \param input    buffer holding the  data
 * \param ilen     length of the input data
 * \param output   SHA-1 checksum result
 */
void aliyun_iot_sha1( const unsigned char *input, size_t ilen, unsigned char output[20] );

#endif
