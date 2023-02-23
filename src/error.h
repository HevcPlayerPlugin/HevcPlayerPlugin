#ifndef __HPP_COMMON_H__
#define __HPP_COMMON_H__

#include <cstdint>
#include "log.h"

typedef enum error_type {
    NoneError = 0,

    InvalidJson = 101,
    InvalidParameter,
    NotSupport,
    InvalidUrl,
    InvalidResolution,
    InvalidWebsocketPort,
    InvalidDumpType,
    PlayVideoError,

    FFOpenUrlFailed = 201,

    WSSendBufferOverflow = 301,

    UnknownError = 999999,
} ErrorType;

const char *GetErrorInfo(uint32_t ErrorNo);

#endif //__HPP_COMMON_H__
