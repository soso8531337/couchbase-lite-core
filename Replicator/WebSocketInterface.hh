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
                                             WebSocketDelegate&) =0;
        virtual void addProtocol(const std::string &protocol) =0;
        virtual void close() { }
    };


    /** Abstract class representing a WebSocket client connection. */
    class WebSocketConnection {
    public:
        WebSocketConnection(WebSocketProvider&, WebSocketDelegate&);
        virtual ~WebSocketConnection();
        virtual void send(fleece::slice message, bool binary =true) =0;
        virtual void close() =0;

        WebSocketProvider& provider() const  {return _provider;}
        WebSocketDelegate& delegate() const  {return _delegate;}

    private:
        WebSocketProvider &_provider;
        WebSocketDelegate &_delegate;
    };


    /** Delegate interface for a WebSocket connection.
        Receives lifecycle events and incoming WebSocket messages.
        These callbacks are made on an undefined thread managed by the WebSocketProvider! */
    class WebSocketDelegate {
    public:
        virtual ~WebSocketDelegate() { }

        WebSocketConnection* connection() const   {return _connection;}

        virtual void onStart()       { }
        virtual void onConnect() =0;
        virtual void onError(int errcode, const char *reason) =0;
        virtual void onClose(int status, fleece::slice reason) =0;

        /** A message has arrived. */
        virtual void onMessage(fleece::slice message, bool binary) =0;

        /** The socket has room to send more messages. */
        virtual void onWriteable()       { }

    private:
        WebSocketConnection* _connection {nullptr};
        friend class WebSocketConnection;
    };



    inline WebSocketConnection::WebSocketConnection(WebSocketProvider &p, WebSocketDelegate &d)
    :_provider(p)
    ,_delegate(d)
    {
        d._connection = this;
    }

    inline WebSocketConnection::~WebSocketConnection() {
        _delegate._connection = nullptr;
    }


}
