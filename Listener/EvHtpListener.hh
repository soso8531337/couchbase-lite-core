//
//  EvHtpListener.hh
//  LiteCore
//
//  Created by Jens Alfke on 12/29/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "Listener.hh"


namespace litecore {

    /** Concrete Listener implementation that uses libevent and libevhtp. */
    class EvHtpListener : public Listener {
    public:
        EvHtpListener(const char *dbsPath)
        :Listener(dbsPath)
        { }

        virtual void run(uint16_t port =59840, const char *address ="0.0.0.0") override;

    protected:
        virtual void sendResponse(Handler*, Response&) override;
    };

}
