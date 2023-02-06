
//#include <vld.h>
#include "signalSession.h"
#include "webSocketServer.h"
#include "config.h"
#include "dump.h"

// hide window in release mode
#ifndef _DEBUG
#pragma comment(linker,"/subsystem:\"Windows\" /entry:\"mainCRTStartup\"")
#endif

SysConfig *gConfig = nullptr;

int main(int argc, char **argv) {
    gConfig = new SysConfig();

    DeclareDumpFile();

    WebsocketServer s(SignalSession::GetInstance());
    s.Run(gConfig->servicePort);

    return 0;
}
