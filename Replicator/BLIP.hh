//
//  BLIP.hh
//  LiteCore
//
//  Created by Jens Alfke on 12/31/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "WebSocketInterface.hh"
#include "Message.hh"
#include <memory>

namespace litecore { namespace blip {
    class BLIPIO;
    class ConnectionDelegate;


    class Connection : public RefCounted<Connection> {
    public:
        Connection(const std::string &hostname, uint16_t port,
                   WebSocketProvider &provider,
                   ConnectionDelegate&);
        virtual ~Connection();

        ConnectionDelegate& delegate() const                    {return _delegate;}

        MessageIn* sendRequest(MessageBuilder&);

        void close();

    protected:
        friend class MessageIn;
        friend class BLIPIO;

        void send(MessageOut*);

    private:
        ConnectionDelegate &_delegate;
        std::unique_ptr<BLIPIO> _io;
    };


    class ConnectionDelegate {
    public:
        virtual ~ConnectionDelegate()  { }

        Connection* connection() const  {return _connection;}

        virtual void onConnect()                                {}
        virtual void onError(int errcode, const char *reason)   =0;
        virtual void onClose(int status, fleece::slice reason)  =0;
        virtual void onRequestReceived(MessageIn*)              =0;
        virtual void onResponseReceived(MessageIn*)             {}

    private:
        friend class Connection;
        
        Connection* _connection;
    };

} }
