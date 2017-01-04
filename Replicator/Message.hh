//
//  Message.hh
//  LiteCore
//
//  Created by Jens Alfke on 1/2/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "RefCounted.hh"
#include "Writer.hh"
#include <functional>
#include <memory>
#include <unordered_map>
#include <queue>

namespace litecore { namespace blip {

    class Connection;
    class BLIPIO;
    class MessageIn;


    enum FrameFlags: uint8_t {
        kRequestType     = 0,  // A message initiated by a peer
        kResponseType    = 1,  // A response to a Request
        kErrorType       = 2,  // A response indicating failure
        kAckRequestType  = 4,  // Acknowledgement of data received from a Request (internal)
        kAckResponseType = 5,  // Acknowledgement of data received from a Response (internal)

        kTypeMask   = 0x07,
        kCompressed = 0x08,
        kUrgent     = 0x10,
        kNoReply    = 0x20,
        kMoreComing = 0x40,
        kMeta       = 0x80,
    };

    typedef std::unordered_map<std::string, std::string> Properties;

    typedef uint64_t MessageNo;


    /** Abstract base class of messages */
    class Message : public RefCounted<Message> {
    public:
        Message(FrameFlags f)               :_flags(f) { }

        FrameFlags flags() const            {return _flags;}

        FrameFlags type() const             {return (FrameFlags)(_flags & kTypeMask);}
        bool hasFlag(FrameFlags f) const    {return (_flags & f) != 0;}
        bool urgent() const                 {return hasFlag(kUrgent);}

    protected:
        FrameFlags _flags;
        MessageNo _number {0};
    };


    /** A temporary object used to construct an outgoing message (request or response),
        which is then converted to a MessageOut when sent. */
    class MessageBuilder {
    public:
        typedef std::pair<slice, slice> property;

        MessageBuilder();
        MessageBuilder(MessageIn *inReplyTo);
        MessageBuilder(std::initializer_list<property>);

        MessageBuilder& addProperty(slice name, slice value);
        MessageBuilder& addProperty(slice name, int value);
        MessageBuilder& addProperties(std::initializer_list<property>);

        void makeError(slice domain, int code, slice message);

        MessageBuilder& write(slice);
        MessageBuilder& operator<< (slice s)  {return write(s);}

        void reset();

        FrameFlags type     {kRequestType};
        bool urgent         {false};
        bool compressed     {false};
        bool noreply        {false};

        FrameFlags flags() const;

    protected:
        friend class MessageOut;

        alloc_slice extractOutput();

    private:
        void finishProperties();

        fleece::Writer _out;
        const void *_propertiesSizePos;
    };


    /** An outgoing message that's been constructed by a MessageBuilder. */
    class MessageOut : public Message {
    protected:
        friend class MessageIn;
        friend class Connection;
        friend class BLIPIO;

        MessageOut(Connection*, MessageBuilder&, MessageNo =0);
        slice nextFrameToSend(size_t maxSize, FrameFlags &outFlags);
        MessageIn* pendingResponse();

    private:
        Connection* const _connection;
        alloc_slice _payload;
        size_t _bytesSent {0};
        Retained<MessageIn> _pendingResponse;
    };


    /** An incoming message. */
    class MessageIn : public Message {
    public:
        slice body() const                          {return _body;}
        slice operator[] (slice property) const;

        std::function<void(MessageIn*)> onComplete;

        void respond(MessageBuilder&);
        void respondWithError(slice domain, int code, slice message);

    protected:
        friend class MessageOut;
        friend class BLIPIO;

        MessageIn(Connection*, FrameFlags, MessageNo);
        bool receivedFrame(slice, FrameFlags);
        void messageComplete();

    private:
        Connection* const _connection;
        std::unique_ptr<fleece::Writer> _in;
        uint32_t _propertiesSize {0};
        alloc_slice _properties;
        alloc_slice _body;
    };

} }
