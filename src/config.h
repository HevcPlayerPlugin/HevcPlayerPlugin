#ifndef __HPP_CONFIG_H__
#define __HPP_CONFIG_H__

class SysConfig {
public:
    SysConfig();

    virtual ~SysConfig() = default;

    int start();

public:
    int servicePort;
    int logLevel;
};

extern SysConfig *gConfig;
#endif
