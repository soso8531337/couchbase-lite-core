//
//  Handler.hh
//  LiteCore
//
//  Created by Jens Alfke on 12/24/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "Request.hh"
#include "c4Database.h"
#include <string>


namespace litecore {

    class Listener;


    /** Handles a single HTTP request by generating a response and returning it to the Listener.
        This class has all the specifics of the Couchbase Lite REST API. */
    class Handler {
    public:
        Handler(Listener&, const Request &req);

        void run();

        const Request& request() const                  {return _req;}

    private:
        Status doGetRoot();
        Status doAllDBs();
        Status doGetDB();
        Status doPutDB();
        Status doAllDocs();
        Status doGetDoc();
        Status doPutDoc();
        Status doPostDoc();

        // Routing:
        typedef Status (Handler::*call_t)();
        struct route {
            const char* name;
            Method      method;
            call_t      call;
        };
        static const route kRootRoutes[];
        static const route kDBRoutes[];

        call_t lookup(const std::string &name, const route routes[]);

        Listener& _listener;
        const Request _req;
        Response _response;
        std::string _dbName;
        std::string _docID;
        C4Database *_db {nullptr};
    };

}
