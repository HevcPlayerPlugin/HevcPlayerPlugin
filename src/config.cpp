#include "config.h"
#include "error.h"
#include <string>
#include "json.h"
#include <fstream>

constexpr auto SERVICE_PORT_DEFAULT = (30060);

SysConfig::SysConfig() : servicePort(SERVICE_PORT_DEFAULT), logLevel(3) {
    start();
}

int SysConfig::start() {
    ifstream in("conf/config.json", ios::binary);
    if (!in.is_open()) {
        return -1;
    }

    Json::Value root;
    try {
        Json::CharReaderBuilder b;
        JSONCPP_STRING errs;
        if (!parseFromStream(b, in, &root, &errs)) {
            return InvalidJson;
        }
        servicePort = root["servicePort"].asInt();
        logLevel = root["logLevel"].asInt();
    }
    catch (Json::Exception &e) {
        return InvalidJson;
    }

#ifdef _DEBUG
    logLevel = 0;
#endif // _DEBUG


    return 0;
}