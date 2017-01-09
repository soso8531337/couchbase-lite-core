//
//  RefCounted.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 8/12/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#pragma once

#include "Error.hh"
#include "Logging.hh"
#include <atomic>

namespace litecore {

    /** Base class that keeps track of the total instance count of all subclasses,
        which is returned by c4_getObjectCount(). */
    class InstanceCounted {
    public:
        static std::atomic_int gObjectCount;
        InstanceCounted()   {++gObjectCount;}
        ~InstanceCounted()  {--gObjectCount;}
    };


    /** Simple thread-safe ref-counting implementation.
        Note: The ref-count starts at 0, so you must call retain() on an instance right after
        constructing it. */
    template <typename SELF>
    struct RefCounted : InstanceCounted {

        int refCount() const { return _refCount; }

        inline SELF* retain() noexcept {
            ++_refCount;
            return (SELF*)this;
        }

        inline void release() noexcept {
            if (--_refCount <= 0)
                dealloc();
        }

    protected:
        /** Destructor is accessible only so that it can be overridden.
            Never call delete, only release! */
        virtual ~RefCounted() {
            if (_refCount > 0) {
                Warn("FATAL: RefCounted object at %p destructed while it still has a refCount of %d",
                     this, (int)_refCount);
                abort();
            }
        }

    private:
        void dealloc() noexcept {
            int newref = _refCount;
            if (newref == 0)
                delete this;
            else if (newref < 0)
                Warn("RefCounted object at %p released too many times; refcount now %d",
                     this, (int)_refCount);
        }

        std::atomic_int _refCount {0};
    };


    /** Simple smart pointer that retains the RefCounted instance it holds. */
    template <typename T>
    class Retained {
    public:
        Retained()                      :_ref(nullptr) { }
        Retained(T *t)                  :_ref(t ? (T*)t->retain() : nullptr) { }
        Retained(const Retained &r)     :_ref((T*)r._ref->retain()) { }
        Retained(Retained &&r)          :_ref(r._ref) {r._ref = nullptr;}
        ~Retained()             {if (_ref) _ref->release();}
        operator T* () const    {return _ref;}
        T* operator-> () const  {return _ref;}

        T* get() const          {return _ref;}

        Retained& operator=(const Retained &r) {
            if (_ref) _ref->release();
            _ref = r._ref;
            if (_ref) _ref->retain();
            return *this;
        }

        Retained& operator= (Retained &&r) {
            if (_ref) _ref->release();
            _ref = r._ref;
            r._ref = nullptr;
            return *this;
        }

private:
        T *_ref;
    };

}
