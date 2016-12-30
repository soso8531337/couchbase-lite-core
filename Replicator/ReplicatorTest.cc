//
//  main.cpp
//  ReplicatorTest
//
//  Created by Jens Alfke on 12/30/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "LibWSProvider.hh"
#include "Replicator.hh"
#include <iostream>

using namespace litecore;

int main(int argc, const char * argv[]) {
    TestReplicator repl;
    LibWSProvider provider;
    __unused auto connection = provider.connect("localhost", 1234, &repl);
    provider.runEventLoop();
    return 0;
}
