//
//  LibWSProvider.hh
//  LiteCore
//
//  Created by Jens Alfke on 12/30/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "WebSocketInterface.hh"
#include <string>
#include <stdint.h>
#include "slice.hh"

struct ws_base_s;

namespace litecore {

    /** libws-based WebSocket provider. */
    class LibWSProvider : public WebSocketProvider {
    public:
        LibWSProvider();
        virtual ~LibWSProvider();
        virtual WebSocketConnection* connect(const std::string &hostname, uint16_t port,
                                             WebSocketDelegate*) override;

        /** Must be called (on a dedicated thread) to start the libevent event loop.
            This function will not return until close() is called. */
        void runEventLoop();

        virtual void close() override;

    private:
        struct ::ws_base_s* base() {return _base;}

        struct ::ws_base_s *_base {nullptr};

        friend class LibWSConnection;
    };

}
