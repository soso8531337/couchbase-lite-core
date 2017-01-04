//
//  BLIPTest.cpp
//  ReplicatorTest
//
//  Created by Jens Alfke on 12/30/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "LibWSProvider.hh"
#include "BLIP.hh"
#include <iostream>

using namespace litecore;
using namespace fleece;


class BlipTest : public litecore::blip::ConnectionDelegate {
public:
    virtual void onConnect() override {
        std::cerr << "** BLIP Connected\n";
        blip::MessageBuilder msg({{"Profile"_sl, "echo"_sl}});
        uint8_t buffer[256];
        for (int i=0; i<256; i++)
            buffer[i] = (uint8_t)i;
        for (int i = 0; i < 100; i++)
            msg << slice(buffer, sizeof(buffer));
        blip::MessageIn *r = connection()->sendRequest(msg);
        r->onComplete = [](blip::MessageIn *response) {
            std::cerr << "** BLIP response onComplete callback\n";
            slice body = response->body();
            for (size_t i = 0; i < body.size; i++) {
                if (body[i] != (i & 0xff)) {
                    Warn("Invalid body; byte at offset %zu is %02x; should be %02x",
                         i, body[i], (unsigned)(i & 0xff));
                }
            }
        };
    }
    virtual void onError(int errcode, const char *reason) override {
        std::cerr << "** BLIP error: " << reason << "(" << errcode << ")\n";
    }
    virtual void onClose(int status, fleece::slice reason) override {
        std::cerr << "** BLIP closed (status " << status << ")\n";
    }

    virtual void onRequestReceived(blip::MessageIn *msg) override {
        std::cerr << "** BLIP request received\n";
    }

    virtual void onResponseReceived(blip::MessageIn *msg) override {
        std::cerr << "** BLIP response received\n";
    }
};


int main(int argc, const char * argv[]) {
    BlipTest test;
    LibWSProvider provider;
    Retained<blip::Connection> connection(new blip::Connection("localhost", 1234, provider, test));
    provider.runEventLoop();
    return 0;
}
