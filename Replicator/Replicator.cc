//
//  Replicator.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/30/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "Replicator.hh"

using namespace std;
using namespace fleece;

namespace litecore {

    
#pragma mark - TEST


    void TestReplicator::onConnect() {
        fprintf(stderr, "** Connected!\n");
        connection().send(slice("hello"), false);
    }

    void TestReplicator::onError(int errcode, const char *reason) {
        fprintf(stderr, "** Error! %s (%d)\n", reason, errcode);
        connection().provider().close();
    }

    void TestReplicator::onClose(int status, slice reason) {
        fprintf(stderr, "** Closing with status %d\n", status);
        connection().provider().close();
    }

    void TestReplicator::onMessage(slice message, bool binary) {
        fprintf(stderr, ">> Message %d: \"%s\"\n", echo_count, ((string)message).c_str());
        echo_count--;

        {
            const char *send_msg = ((echo_count % 2) == 0) ? "Hello" : "World";
            connection().send(slice(send_msg), false);
        }

        if (echo_count <= 0) {
            printf("** Got last echo\n");
            connection().close();
        }
    }

}
