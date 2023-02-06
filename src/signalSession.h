#ifndef __SIGNAL_SESSION_H__
#define __SIGNAL_SESSION_H__

#include <string>
#include <cstdint>
#include <map>
#include "ffmpegWrapper.h"
#include "webSocketServer.h"
#include "error.h"
#include "json.h"

typedef enum api_type {
    API_Forbidden = 0,

    API_Play,
    API_Stop,
    API_ChangeResolution,
    API_DiscardFrame,
    API_GetVersion,

} APIType;

class SignalSession : public ISession {
public:
    using SignalSessionPtr = std::shared_ptr<SignalSession>;
    using FfmpegWrapperPtr = std::shared_ptr<FfmpegWrapper>;

    ~SignalSession() override = default;

    static SignalSessionPtr GetInstance() {
        static SignalSessionPtr intance = SignalSessionPtr(new SignalSession());
        return intance;
    }

    void OpenFunc(WebsocketServer *, uintptr_t) override {};

    int MessageFunc(WebsocketServer *ws, uintptr_t hdl, const std::string &msg) override;

    void ErrorFunc(WebsocketServer *ws, uintptr_t hdl) override;

    void CloseFunc(WebsocketServer *ws, uintptr_t hdl) override;

protected:
    void stopFFmpeg(WebsocketServer *, uintptr_t);

    int parseAndProcess(WebsocketServer *ws, uintptr_t hdl, const std::string &msg, Json::Value &responseBody);

    int playVideo(WebsocketServer *ws, uintptr_t hdl, const Json::Value &jsonRequest);

    int changeVideoResolution(uintptr_t hdl, const Json::Value &jsonRequest);

    int stopPlay(uintptr_t hdl, const Json::Value &jsonRequest);

    int discardFrame(uintptr_t hdl, const Json::Value &jsonRequest);

    std::string getVersion();

private:
    struct MediaResource {
        std::string url;
        FfmpegWrapperPtr ffmpegWrapper;

        MediaResource(std::string &&s, const FfmpegWrapperPtr &p) {
            url = std::move(s);
            ffmpegWrapper = p;
        }
    };

    SignalSession() = default;

    std::mutex mu_;
    std::map<uintptr_t, MediaResource> mediaResourceManager_;
};

#endif //__SIGNAL_SESSION_H__
