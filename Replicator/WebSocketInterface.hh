//
//  WebSocketInterface.hh
//  LiteCore
//
//  Created by Jens Alfke on 12/30/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "slice.hh"

namespace litecore {

    class WebSocketConnection;
    class WebSocketDelegate;


    /** Abstract class that can open WebSocket client connections. */
    class WebSocketProvider {
    public:
        virtual ~WebSocketProvider() { }
        virtual WebSocketConnection* connect(const std::string &hostname,
                                             uint16_t port,
                                             WebSocketDelegate*) =0;

        virtual void close() { }
    };


    /** Abstract class representing a WebSocket client connection. */
    class WebSocketConnection {
    public:
        WebSocketConnection(WebSocketProvider &p)
        :_provider(p)
        { }

        virtual ~WebSocketConnection() { }
        virtual void send(fleece::slice message, bool binary =true) =0;
        virtual void close() =0;

        WebSocketProvider& provider() const  {return _provider;}

    private:
        WebSocketProvider &_provider;
    };


    /** Delegate interface for a WebSocket connection.
        Receives lifecycle events and incoming WebSocket messages. */
    class WebSocketDelegate {
    public:
        virtual ~WebSocketDelegate() { }
        virtual void onStart(WebSocketConnection* connection)       {_connection = connection;}
        virtual void onConnect() =0;
        virtual void onError(int errcode, const char *reason) =0;
        virtual void onClose(int status, fleece::slice reason) =0;
        virtual void onMessage(fleece::slice message, bool binary) =0;

        WebSocketConnection& connection() const   {return *_connection;}

    private:
        WebSocketConnection* _connection {nullptr};
    };

}
