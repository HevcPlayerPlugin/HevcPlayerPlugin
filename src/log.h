#pragma once

#include <iostream>
#include <ctime>
#include <thread>
#include <cstring>
#include <sstream>
#include "config.h"

using namespace std;

typedef enum {
    Debug = 0, Info, Warning, Error, Fatal,
} LogLevel;

class Logger {
public:
    explicit Logger(int level, const char *tag, const std::string &filename = "");

    virtual ~Logger();

    template<typename T>
    friend const Logger &operator<<(const Logger &in2, T &&data) {
        std::string in = static_cast<const std::stringstream &>(std::stringstream() << data).str();
        if (in2.m_nLevel >= gConfig->logLevel) {
            if (in2.fp)
                fwrite(in.c_str(), in.length(), 1, in2.fp);
            cout << in;
        }
        return in2;
    }

private:
    void openLogFile();

    FILE *fp = nullptr;
    int m_nLevel;
};

#define LOG_DEBUG (Logger(Debug, "Debug"))
#define LOG_INFO  (Logger(Info, "Info"))
#define LOG_WARN  (Logger(Warning, "Warning"))
#define LOG_ERROR (Logger(Error, "Error"))
#define LOG_FATAL (Logger(Fatal, "Fatal"))
