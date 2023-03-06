#pragma once

#ifdef _WIN32
// 下面这俩货顺序不能变
#include <Windows.h>
#include <DbgHelp.h>

#pragma comment(lib, "Dbghelp.lib")

static LONG WINAPI CreateDmpFile(struct _EXCEPTION_POINTERS *pExPointInfo) {
    SYSTEMTIME stLocalTime;
    GetLocalTime(&stLocalTime);

    TCHAR szFileName[MAX_PATH] = {0};
    wsprintf(szFileName, "%s-%04d%02d%02d-%02d%02d%02d-%ld-%ld.dmp",
            //szPath, szAppName, szVersion,
             "core",
             stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
             stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
             GetCurrentProcessId(), GetCurrentThreadId());

    HANDLE hFile = CreateFileA(szFileName, GENERIC_WRITE,
                               FILE_SHARE_WRITE, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION miniExInfo;

        miniExInfo.ThreadId = ::GetCurrentThreadId();
        miniExInfo.ExceptionPointers = pExPointInfo;
        miniExInfo.ClientPointers = NULL;

        BOOL bOk = MiniDumpWriteDump(GetCurrentProcess(),
                                     GetCurrentProcessId(),
                                     hFile,
                                     MiniDumpNormal,
                                     &miniExInfo,
                                     nullptr,
                                     nullptr);
        CloseHandle(hFile);
        return bOk ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER;
    }

    return EXCEPTION_EXECUTE_HANDLER;
}

void DeclareDumpFile() {
    SetUnhandledExceptionFilter(CreateDmpFile);
}
#endif