//
//  LibWSProvider.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/30/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "LibWSProvider.hh"

#include <libws.h>
#include <libws_log.h>

#include <inttypes.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <mutex>
#include <thread>

using namespace std;
using namespace fleece;

namespace litecore {


#pragma mark - CONNECTION:


    /** libws-based WebSocket connection. */
    class LibWSConnection : public WebSocketConnection {
    public:

        LibWSConnection(LibWSProvider &provider, ws_t websocket,
                        const std::string &hostname, uint16_t port,
                        WebSocketDelegate &delegate)
        :WebSocketConnection(provider, delegate)
        ,_ws(websocket)
        {
            ws_set_onwrite_cb(_ws, onwrite, &delegate);
            ws_set_onmsg_cb(_ws, onmsg, &delegate);
            ws_set_onconnect_cb(_ws, onconnect, &delegate);
            ws_set_onclose_cb(_ws, onclose, &delegate);
            ws_set_onerr_cb(_ws, onerror, &delegate);
            
            if (ws_connect(_ws, hostname.c_str(), port, "")) {
                ws_destroy(&_ws);
                throw "connection failed";
            }
            delegate.onStart();
        }

        ~LibWSConnection() {
            ws_destroy(&_ws);
        }

        void close() {
            ws_close(_ws);
        }


        void send(slice msg, bool binary) {
            slice mutableMessage = msg.copy();
            if (ws_send_msg_ex(_ws, (char*)mutableMessage.buf, mutableMessage.size, binary) != 0)
                throw "WebSocket message send failed";
            mutableMessage.free();
        }


        static void onconnect(ws_t ws, void *context) noexcept {
            try {
                ((WebSocketDelegate*)context)->onConnect();
            } catch (...) {
                fprintf(stderr, "WARNING: WebSocketDelegate::onConnect threw an exception\n");
            }
        }

        static void onwrite(ws_t ws, void *context) noexcept {
            try {
                ((WebSocketDelegate*)context)->onWriteable();
            } catch (...) {
                fprintf(stderr, "WARNING: WebSocketDelegate::onWriteable threw an exception\n");
            }
        }

        static void onmsg(ws_t ws, char *msg, uint64_t len, int binary, void *context) noexcept {
            try {
                ((WebSocketDelegate*)context)->onMessage({msg, len}, binary);
            } catch (...) {
                fprintf(stderr, "WARNING: WebSocketDelegate::onMessage threw an exception\n");
            }
        }

        static void onclose(ws_t ws, ws_close_status_t status,
                            const char *reason, size_t reason_len,
                            void *context) noexcept
        {
            try {
                ((WebSocketDelegate*)context)->onClose(status, {reason, reason_len});
            } catch (...) {
                fprintf(stderr, "WARNING: WebSocketDelegate::onClose threw an exception\n");
            }
        }

        static void onerror(ws_t ws, int errcode, const char *errmsg, void *context) noexcept {
            try {
                ((WebSocketDelegate*)context)->onError(errcode, errmsg);
            } catch (...) {
                fprintf(stderr, "WARNING: WebSocketDelegate::onError threw an exception\n");
            }
        }

    private:
        ws_t _ws {nullptr};
    };


#pragma mark - PROVIDER:


    LibWSProvider::LibWSProvider() {
        //FIX: Make this thread-safe (wrap a critical section around it)
        static bool sInitialized = false;
        if (!sInitialized) {
            sInitialized = true;
            int level = LIBWS_CRIT | LIBWS_ERR | LIBWS_WARN;
            ws_set_log_cb(ws_default_log_cb);
            if (getenv("WSLog"))
                level |= LIBWS_INFO | LIBWS_DEBUG;
            ws_set_log_level(level);
        }
        if (ws_global_init(&_base) != 0)
            throw "Failed to init ws_base";
    }


    LibWSProvider::~LibWSProvider() {
        if (_base)
            ws_global_destroy(&_base);
    }


    void LibWSProvider::addProtocol(const std::string &protocol) {
        if (find(_protocols.begin(), _protocols.end(), protocol) == _protocols.end())
            _protocols.push_back(protocol);
    }


    WebSocketConnection* LibWSProvider::connect(const std::string &hostname, uint16_t port,
                                                WebSocketDelegate &delegate)
    {
        ws_t ws;
        if (ws_init(&ws, _base) != 0)
            throw "Failed to init websocket state";
        for (auto &proto : _protocols)
            ws_add_subprotocol(ws, proto.c_str());
        return new LibWSConnection(*this, ws, hostname, port, delegate);
    }

    void LibWSProvider::runEventLoop() {
        ws_base_service_blocking(_base);
    }

    void LibWSProvider::startEventLoop() {
        if (!_eventLoopThread)
            _eventLoopThread.reset( new std::thread([this]{runEventLoop();}) );
    }

    void LibWSProvider::stopEventLoop() {
        ws_base_quit(_base, true);
    }

    void LibWSProvider::close() {
        stopEventLoop();
        if (_eventLoopThread)
            _eventLoopThread->join();
    }

}
