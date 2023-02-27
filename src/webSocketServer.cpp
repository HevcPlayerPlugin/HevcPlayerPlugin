#include "webSocketServer.h"
#include "error.h"
#include "ffmpegWrapper.h"

WebsocketServer::WebsocketServer(const std::shared_ptr<ISession> &ptr) {
    sessionHander_ = ptr;
    init();
}

WebsocketServer::WebsocketServer(ISession *ptr) {
    sessionHander_ = std::shared_ptr<ISession>(ptr);
    init();
}

void WebsocketServer::init() {
// Set logging settings
    endpoint_.set_error_channels(websocketpp::log::elevel::fatal);
    endpoint_.set_max_message_size(3 * 32000000);
    //endpoint_.set_access_channels(websocketpp::log::alevel::all ^ websocketpp::log::alevel::frame_payload);
    endpoint_.clear_access_channels(websocketpp::log::alevel::all/*frame_payload*/);

    // Initialize Asio
    endpoint_.init_asio();

    // Set the default message handler to the echo handler
    endpoint_.set_message_handler(std::bind(
            &WebsocketServer::onMessage, this,
            std::placeholders::_1, std::placeholders::_2
    ));


    endpoint_.set_open_handler(websocketpp::lib::bind(
            &WebsocketServer::onOpen,
            this,
            websocketpp::lib::placeholders::_1
    ));

    endpoint_.set_fail_handler(websocketpp::lib::bind(
            &WebsocketServer::onError,
            this,
            websocketpp::lib::placeholders::_1
    ));

    endpoint_.set_close_handler(websocketpp::lib::bind(
            &WebsocketServer::onClose,
            this,
            websocketpp::lib::placeholders::_1
    ));
}

void WebsocketServer::onOpen(WebsocketCon hdl) {
    Server::connection_ptr con = endpoint_.get_con_from_hdl(hdl);
    uintptr_t key = (uintptr_t) &con->get_socket();
    mutex_.lock();
    mapConnection_.insert(std::pair<uintptr_t, WebsocketCon>(key, hdl));
    mutex_.unlock();
    if (sessionHander_) {
        sessionHander_->OpenFunc(this, key);
    }
}

void WebsocketServer::onMessage(WebsocketCon hdl, Server::message_ptr msg) {
    Server::connection_ptr con = endpoint_.get_con_from_hdl(hdl);
    uintptr_t key = (uintptr_t) &con->get_socket();
    {
        std::lock_guard<std::shared_mutex> lk(mutex_);
        auto iter = mapConnection_.find(key);
        if (iter == mapConnection_.end()) {
            LOG_ERROR << "Receive Msg:" << msg->get_payload();
            return;
        }
    }
    if (sessionHander_ && sessionHander_->MessageFunc(this, key, msg->get_payload())) {
        LOG_ERROR << "message callback error";
    }
//    if (mFunc_ && mFunc_(this, key, msg->get_payload())) {
//        LOG_ERROR << "message callback error";
//    }
}

void WebsocketServer::onError(WebsocketCon hdl) {
    Server::connection_ptr con = endpoint_.get_con_from_hdl(hdl);
    LOG_ERROR << "Fail handler: " << con->get_ec().value() << " " << con->get_ec().message();

    uintptr_t key = (uintptr_t) &con->get_socket();
    {
        std::lock_guard<std::shared_mutex> lk(mutex_);
        auto iter = mapConnection_.find(key);
        if (iter == mapConnection_.end()) {
            LOG_ERROR << "closeHandle: Cannot find the key in map";
            return;
        }
    }

    if (sessionHander_) {
        sessionHander_->ErrorFunc(this, key);
    }
    mutex_.lock();
    mapConnection_.erase(key);
    mutex_.unlock();
//    closeHandle(hdl);
}

void WebsocketServer::onClose(WebsocketCon hdl) {
    std::cout << "Client close" << std::endl;

    Server::connection_ptr con = endpoint_.get_con_from_hdl(hdl);
    uintptr_t key = (uintptr_t) &con->get_socket();
    {
        std::lock_guard<std::shared_mutex> lk(mutex_);
        auto iter = mapConnection_.find(key);
        if (iter == mapConnection_.end()) {
            LOG_ERROR << "closeHandle: Cannot find the key in map";
            return;
        }
    }

    if (sessionHander_) {
        sessionHander_->CloseFunc(this, key);
    }
    mutex_.lock();
    mapConnection_.erase(key);
    mutex_.unlock();
}

int WebsocketServer::Send(uintptr_t key, uint8_t *buf, unsigned int len, OpCode code) {
    try {
        websocketpp::lib::error_code err;
        WebsocketCon hdl;
        {
            std::shared_lock<std::shared_mutex> sl(mutex_);
            auto iter = mapConnection_.find(key);
            if (iter == mapConnection_.end()) {
                LOG_ERROR << "WebsocketServer::Send: Cannot find the key in map";
                return -1;
            }
            hdl = iter->second;
        }

        Server::connection_ptr con = endpoint_.get_con_from_hdl(hdl);
        while (con->get_buffered_amount() > 0 && buf[0] == 0x1) {
            return WSSendBufferOverflow;
        }

        endpoint_.send(hdl, buf, len, code, err);
        if (err) {
            LOG_ERROR << "send error " << err.value() << "(" << err.message() << ")";
            return -1;
        }
    }
    catch (...) {
        LOG_ERROR << "WebsocketServer Send error ";
    }

    return 0;
}

void WebsocketServer::Run(int port) {
    try {
        websocketpp::lib::error_code ec;
        endpoint_.listen(port, ec);
        if (ec) {
            LOG_ERROR << "[" << ec.value() << "]" << ec.message();
            if (ec.value() == 10048) {
                LOG_ERROR << "请修改配置文件中WebsocketPort的值";
            }
            return;
        }

        std::cout << "Listen at 127.0.0.1:" << port << endl;
        // Queues a connection accept operation
        endpoint_.start_accept();
        // Start the Asio io_service run loop
        endpoint_.run();
    } catch (const std::exception &e) {
        LOG_ERROR << e.what();
        return;
    }
}