//
//  ActorTest.cc
//  LiteCore
//
//  Created by Jens Alfke on 1/8/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "Actor.hh"
#include "Catch.hpp"

using namespace std;
using namespace litecore;


class Adder : public Actor {
public:
    Adder(Scheduler *s)                 :Actor(s), total(_total), _total(this) { }

    void setName(string name)           {enqueue(&Adder::_setName, name);}
    void add(int a, int b)              {enqueue(&Adder::_add, a, b);}

    Property<int> total;

private:

    ~Adder() {
        Log("DELETE Actor<%p>", this);
    }

    void _setName(string name) {
        Log("Actor<%p> is named %s", this, name.c_str());
        _name = name;
    }

    void _add(int a, int b) {
        _total = _total + a * b;
        Log("Actor<%p> %s: Adding %d x %d gives total %d", this, _name.c_str(), a, b, _total.get());

        if (b > 1)
            add(a, b-1);
        if (a > 1)
            add(a-1, 10);
        //usleep(100*1000);
    }

    PropertyImpl<int> _total;

    string _name;
};



TEST_CASE("Actor") {
    Scheduler s;
    Retained<Adder> a(new Adder(&s));
    Retained<Adder> b(new Adder(&s));
    a->setName("A");
    b->setName("B");
    a->add(3, 4);
    a->add(5, 6);
    b->add(3, 4);
    b->add(5, 6);
    a = nullptr;
    b = nullptr;
    Log("Starting scheduler...");

#if 1
    s.runSynchronous();
#else
    s.start(2);
    this_thread::sleep_for(seconds(2));
    fprintf(stderr, "Stopping scheduler...\n");
    s.stop();
#endif

    Log("exiting...");
}
