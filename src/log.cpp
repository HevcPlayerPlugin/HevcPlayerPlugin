#include "log.h"
#include "config.h"

Logger::Logger(int level, const char *tag, const std::string &filename /*= ""*/) 
    : fp(nullptr) {
    if ((m_nLevel = level) < gConfig->logLevel) {
        return;
    }

    try {
        if (!fp) {
            openLogFile();
        }

        if (fp) {
            struct tm t;
            time_t now = time(0);
            localtime_s(&t, &now);

            char header[256] = {0};
            snprintf(header, sizeof(header), "%04d-%02d-%02d %02d:%02d:%02d.%09d.[%s][%d]:",
                     t.tm_year + 1900,
                     t.tm_mon + 1,
                     t.tm_mday,
                     t.tm_hour,
                     t.tm_min,
                     t.tm_sec,
                     clock(), tag, std::hash<std::thread::id>{}(std::this_thread::get_id()));

            fwrite(header, strlen(header), 1, fp);
            cout << header;
        }
    }
    catch (const std::exception &e) {
        auto r = e.what();
        cout << r << endl;
    }
}

Logger::~Logger() {
    if (fp) {
        fwrite("\n", 1, 1, fp);
        fflush(fp);
        fclose(fp);
        fp = nullptr;
    }
    if (m_nLevel >= gConfig->logLevel) {
        cout << '\n';
    }
}

void Logger::openLogFile() {
    struct tm t;
    time_t now = time(0);
    localtime_s(&t, &now);

    char filename[256] = {0};
    snprintf(filename, sizeof(filename), "%04d-%02d.log",
             t.tm_year + 1900,
             t.tm_mon + 1);

    fp = fopen(filename, "ab+");
}
