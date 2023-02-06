
#include <string>
#include "error.h"

typedef struct error_msg {
    uint32_t ErrorCode;
    const char *ErrorMsg;
} ERROR_MSG;
const ERROR_MSG g_ErrorMsg[] = {
        {NoneError,            "Success"},
        {InvalidJson,          "Invalid Json"},
        {NotSupport,           "Request Not Support"},
        {InvalidUrl,           "Invalid Video Url"},
        {InvalidResolution,    "Invalid Video Resolution"},
        {InvalidWebsocketPort, "Invalid Websocket port"},
        {InvalidDumpType,      "Invalid Core Dump Type"},
        {PlayVideoError,       "Play Video Error"},

        {FFOpenUrlFailed,      "Could not open source file "},

        {WSSendBufferOverflow, "send buffer overflow, check CPU please."},

        {UnknownError,         "Unknown Error"},
};

const char *GetErrorInfo(uint32_t ErrorNo) {
    int iPos = 0;
    while (UnknownError != g_ErrorMsg[iPos].ErrorCode) {
        if (ErrorNo == g_ErrorMsg[iPos].ErrorCode) {
            return g_ErrorMsg[iPos].ErrorMsg;
        }
        iPos++;
    }
    return "Unknown Error";
}

