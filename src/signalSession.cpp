#include "signalSession.h"
#include "version.h"
#include <memory>

const int YUV444_4K = 3840 * 2160 * 3;

int SignalSession::MessageFunc(WebsocketServer *ws, uintptr_t hdl, const std::string &msg) {
    LOG_DEBUG << "msg:" << msg;

    Json::Value responseBody;
    int code = parseAndProcess(ws, hdl, msg, responseBody);
    ws->Send(hdl, (uint8_t *) responseBody.toStyledString().data(), responseBody.toStyledString().size(),
             WebsocketServer::OpCode::text);

    return code == NoneError ? 0 : -1;
}

void SignalSession::ErrorFunc(WebsocketServer *ws, uintptr_t hdl) {
    stopFFmpeg(ws, hdl);
}

void SignalSession::CloseFunc(WebsocketServer *ws, uintptr_t hdl) {
    stopFFmpeg(ws, hdl);
}

void SignalSession::stopFFmpeg(WebsocketServer *ws, uintptr_t hdl) {
    FfmpegWrapperPtr ffPtr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto iter = mediaResourceManager_.find(hdl);
        if (iter == mediaResourceManager_.end()) {
            return;
        }
        ffPtr = iter->second.ffmpegWrapper;
        mediaResourceManager_.erase(iter);
    }
    if (ffPtr) {
        ffPtr->stopPlay();
    }
}

int
SignalSession::parseAndProcess(WebsocketServer *ws, uintptr_t hdl, const std::string &msg, Json::Value &responseBody) {
    int code = NoneError;

    Json::Value jsonRequest;
    int type = 0;
    try {
        Json::CharReaderBuilder b;
        Json::CharReader *reader(b.newCharReader());
        std::string errs;
        if (!reader->parse(msg.c_str(), msg.c_str() + msg.size(), &jsonRequest, &errs)) {
            responseBody["result"] = InvalidJson;
            responseBody["message"] = GetErrorInfo(InvalidJson);
            return InvalidJson;
        }
        type = jsonRequest["type"].asInt();
        delete reader;
    } catch (Json::Exception &e) {
        LOG_DEBUG << "body:" << msg;
        LOG_ERROR << "Parse Json Error:" << e.what();
        responseBody["result"] = InvalidJson;
        responseBody["message"] = GetErrorInfo(InvalidJson);
        return InvalidJson;
    }

    switch (type) {
        case API_Play:
            code = playVideo(ws, hdl, jsonRequest);
            break;
        case API_ChangeResolution:
            code = changeVideoResolution(hdl, jsonRequest);
            break;
        case API_Stop:
            code = stopPlay(hdl, jsonRequest);
            break;
        case API_DiscardFrame:
            code = discardFrame(hdl, jsonRequest);
            break;
        case API_GetVersion: 
            responseBody["version"] = getVersion();
            break;
        default:
            code = NotSupport;
    }
    responseBody["type"] = type;
    responseBody["result"] = code;
    responseBody["message"] = GetErrorInfo(code);
    return code;
}

int SignalSession::playVideo(WebsocketServer *ws, uintptr_t hdl, const Json::Value &jsonRequest) {
    uint16_t width, height, useGPU = 0;
    uint16_t useTCP = 1;
    std::string url;

    try {
        if (!jsonRequest.isMember("param")) {
            return NotSupport;
        }
        Json::Value playParam = jsonRequest["param"];
        url = playParam["url"].asString();
        if (url.empty()) {
            return InvalidUrl;
        }
        useGPU = playParam["use_gpu"].asUInt();
        width = playParam["width"].asUInt();
        height = playParam["height"].asUInt();
        if (!jsonRequest.isMember("use_tcp")) {
            useTCP = playParam["use_tcp"].asUInt();
        }
    } catch (Json::Exception &e) {
        LOG_ERROR << "Parse Json Error:" << e.what();
        return InvalidJson;
    }

    FfmpegWrapperPtr ffPtr = std::make_shared<FfmpegWrapper>();
    ffPtr->setSendDataCallback((void *) ws, hdl, [](void *user, uintptr_t handle, uint8_t *data, size_t length) -> int {
        if (length > YUV444_4K) {
            LOG_ERROR << " Msg too large to send(size=" << length << ")";
            return -1;
        }
        WebsocketServer *ws = static_cast<WebsocketServer *>(user);
        return ws->Send(handle, data, length);
    });
    ffPtr->setExceptionCallback((void *) ws, hdl,
                                [](void *user, uintptr_t handle, int err_code, const uint8_t *err_desc) -> int {
                                    Json::Value responseBody;
                                    responseBody["result"] = err_code;
                                    responseBody["message"] = (const char *) err_desc;
                                    WebsocketServer *ws = static_cast<WebsocketServer *>(user);
                                    return ws->Send(handle, (uint8_t *) responseBody.toStyledString().c_str(),
                                                    responseBody.toStyledString().size(),
                                                    WebsocketServer::OpCode::text);
                                });

    int ret = UnknownError;
    if ((ret = ffPtr->startPlay(url.c_str(), width, height, useGPU)) != NoneError) {
        return ret;
    }

    mu_.lock();
    mediaResourceManager_.insert(std::make_pair(hdl, MediaResource(std::move(url), ffPtr)));
    mu_.unlock();

    return NoneError;
}

int SignalSession::changeVideoResolution(uintptr_t hdl, const Json::Value &jsonRequest) {
    uint16_t width = -1;
    uint16_t height = -1;
    try {
        if (!jsonRequest.isMember("param")) {
            return NotSupport;
        }
        Json::Value playParam = jsonRequest["param"];
        width = playParam["width"].asUInt();
        height = playParam["height"].asUInt();
    } catch (Json::Exception &e) {
        LOG_ERROR << "Parse Json Error:" << e.what();
        return InvalidJson;
    }


    std::lock_guard<std::mutex> lk(mu_);
    auto iter = mediaResourceManager_.find(hdl);
    if (iter == mediaResourceManager_.end()) {
        return NoneError;
    }
    iter->second.ffmpegWrapper->changeVideoResolution(width, height);
    return NoneError;
}

int SignalSession::stopPlay(uintptr_t hdl, const Json::Value &jsonRequest) {
    FfmpegWrapperPtr ffPtr;
    {
        std::lock_guard<std::mutex> lk(mu_);
        auto iter = mediaResourceManager_.find(hdl);
        if (iter == mediaResourceManager_.end()) {
            return NoneError;
        }

        ffPtr = iter->second.ffmpegWrapper;
        mediaResourceManager_.erase(iter);
    }
    if (ffPtr) {
        ffPtr->stopPlay();
    }
    return NoneError;
}

int SignalSession::discardFrame(uintptr_t hdl, const Json::Value &jsonRequest) {
    int enabled = -1;
    try {
        if (!jsonRequest.isMember("param")) {
            return NotSupport;
        }
        Json::Value playParam = jsonRequest["param"];
        enabled = playParam["enabled"].asUInt();
        if (enabled < 0) {
            return InvalidParameter;
        }
    }
    catch (Json::Exception &e) {
        LOG_ERROR << "Parse Json Error:" << e.what();
        return InvalidJson;
    }

    std::lock_guard<std::mutex> lk(mu_);
    auto iter = mediaResourceManager_.find(hdl);
    if (iter == mediaResourceManager_.end()) {
        return NoneError;
    }
    iter->second.ffmpegWrapper->openDiscardFrames(enabled);
    return NoneError;
}

std::string SignalSession::getVersion() {
    return STRING_FULL_VERSION;
}
