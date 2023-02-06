#pragma once

#define ASIO_STANDALONE
#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <shared_mutex>
#include <functional>

typedef websocketpp::server<websocketpp::config::asio> Server;
using WebsocketCon = websocketpp::connection_hdl;

class ISession;

class WebsocketServer {
public:
    using OpCode = websocketpp::frame::opcode::value;

    explicit WebsocketServer(const std::shared_ptr<ISession> &ptr);

    explicit WebsocketServer(ISession *ptr);

    virtual ~WebsocketServer() = default;;

    int Send(uintptr_t key, uint8_t *buf, unsigned int len, OpCode code = OpCode::binary);

    void Run(int port);

protected:
    void init();

    void onOpen(WebsocketCon hdl);

    void onMessage(WebsocketCon hdl, Server::message_ptr msg);

    void onError(WebsocketCon hdl);

    void onClose(WebsocketCon hdl);

private:
    Server endpoint_;

    std::shared_mutex mutex_;
    std::map<uintptr_t, WebsocketCon> mapConnection_;
    std::shared_ptr<ISession> sessionHander_;
};

class ISession {
public:
    ISession() = default;

    virtual ~ISession() = default;

public:
    virtual void OpenFunc(WebsocketServer *, uintptr_t) = 0;

    virtual int MessageFunc(WebsocketServer *, uintptr_t, const std::string &) = 0;

    virtual void ErrorFunc(WebsocketServer *, uintptr_t) = 0;

    virtual void CloseFunc(WebsocketServer *, uintptr_t) = 0;
};

