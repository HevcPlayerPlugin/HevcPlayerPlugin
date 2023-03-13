
//#include <vld.h>
#include "signalSession.h"
#include "webSocketServer.h"
#include "config.h"
#include "dump.h"

// hide window in release mode
#ifdef WIN32
#ifndef _DEBUG
#pragma comment(linker,"/subsystem:\"Windows\" /entry:\"mainCRTStartup\"")
#endif
#endif

SysConfig *gConfig = nullptr;

int main(int argc, char **argv) {
    gConfig = new SysConfig();

#ifdef WIN32
    DeclareDumpFile();
#else
//	todo
#endif
    WebsocketServer s(SignalSession::GetInstance());
    s.Run(gConfig->servicePort);

    return 0;
}
