//
//  EvHtpListener.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/29/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//    http://www.apache.org/licenses/LICENSE-2.0
//  Unless required by applicable law or agreed to in writing, software distributed under the
//  License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
//  either express or implied. See the License for the specific language governing permissions
//  and limitations under the License.

// https://github.com/ellzey/libevhtp

#include "EvHtpListener.hh"
#include "Handler.hh"
#include "evhtp.h"
#include <assert.h>

using namespace std;

namespace litecore {

    static void kvToMap(evhtp_kvs_t *kvs, Map &map) {
        if (kvs) {
            for (auto i = kvs->tqh_first; i; i = i->next.tqe_next) {
                map.set(string(i->key, i->klen), string(i->val, i->vlen));
            }
        }
    }


    static void requestHook(evhtp_request_t * ev_req, void * context) {
        // Extract the path components:
        vector<string> pathComponents;
        pathComponents.reserve(2);
        const char *path = ev_req->uri->path->full;
        auto bufSize = strlen(path);
        auto buf = new uint8_t[bufSize];
        while (path[0]) {
            assert(path[0] == '/');
            ++path;
            const char *slash = strchr(path, '/');
            if (!slash)
                slash = path + strlen(path);
            memset(buf, 0, bufSize);    // Workaround for a bug in evhtp_unescape_string
            evhtp_unescape_string(&buf, (uint8_t*)path, slash - path);
            path = slash;
            pathComponents.push_back(string((char*)buf));
        }
        delete []buf;

        // Create the Request object:
        Request req((Method)ev_req->method, pathComponents, ev_req);
        kvToMap(ev_req->uri->query, req.queries);
        kvToMap(ev_req->headers_in, req.headers);

        auto h = new Handler(*(Listener*)context, req);
        h->run();
    }


    void EvHtpListener::run(uint16_t port, const char *address) {
        evbase_t * evbase = event_base_new();
        evhtp_t  * htp    = evhtp_new(evbase, NULL);
        if (!htp) throw "evhtp_new failed";

        // Register handlers:
        evhtp_set_glob_cb(htp, "/*",            requestHook,      this);

        if (evhtp_bind_socket(htp, address, port, 1024) != 0)
            throw "evhtp_bind_socket failed";
        if (event_base_loop(evbase, 0) < 0)
            throw "event_base_loop failed";
    }


    void EvHtpListener::sendResponse(Handler *handler, Response &response) {
        auto ev_req = (evhtp_request_t*)handler->request().impl;
        for (auto &header : response) {
            auto kv = evhtp_kv_new(header.first.c_str(), header.second.c_str(), true, true);
            evhtp_headers_add_header(ev_req->headers_out, kv);
        }
        {
            auto body = response.extractOutput();
            evbuffer_add(ev_req->buffer_out, body.buf, body.size);
        }
        evhtp_send_reply(ev_req, (evhtp_res)response.status);
        delete handler;
    }

}
