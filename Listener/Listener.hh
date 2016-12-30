//
//  Listener.hh
//  LiteCore
//
//  Created by Jens Alfke on 12/23/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "c4Database.h"
#include <stdint.h>
#include <string>
#include <map>

namespace litecore {
    class Handler;
    class Response;

    /** Top-level HTTP listener. Responsible for opening a listener socket, receiving connections,
        dispatching each request to a Listener, and sending the responses back to the client.

        This is an abstract class; subclasses have to override a few methods to do the real work. */
    class Listener {
    public:
        Listener(const std::string &dbsPath);
        virtual ~Listener();

        virtual void run(uint16_t port =59840, const char *address ="0.0.0.0") =0;

    protected:
        virtual void sendResponse(Handler*, Response&) =0;

        C4Database* getDatabase(const char *name)       {return getDatabase(name, false);}
        C4Database* createDatabase(const char *name)    {return getDatabase(name, true);}

    private:
        virtual C4Database* getDatabase(const char *name, bool createOnly);

        std::string _dbsPath;
        std::map<std::string, C4Database*> _dbs;

        friend class Handler;
    };

}
