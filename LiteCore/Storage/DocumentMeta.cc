//
//  DocumentMeta.cc
//  LiteCore
//
//  Created by Jens Alfke on 12/8/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "DocumentMeta.hh"
#include "Error.hh"
#include "Fleece.hh"

using namespace fleece;


namespace litecore {

    DocumentMeta::DocumentMeta(DocumentFlags f, slice v, slice t)
    :flags(f)
    ,version(v)
    ,docType(t)
    { }

    DocumentMeta::DocumentMeta(slice metaBytes) {
        decode(metaBytes);
    }

    void DocumentMeta::decode(slice metaBytes) {
        if (!metaBytes) {
            flags = DocumentFlags::kNone;
            version = docType = nullslice;
            return;
        }
        auto metaValue = fleece::Value::fromTrustedData(metaBytes);
        if (!metaValue || !metaValue->asArray())
            error::_throw(error::CorruptRevisionData);
        fleece::Array::iterator meta(metaValue->asArray());
        if (meta.count() < 3)
            error::_throw(error::CorruptRevisionData);
        flags = (DocumentFlags)meta.read()->asUnsigned();
        version = meta.read()->asString();
        docType = meta.read()->asString();
        if (docType.size == 0)
            docType.buf = nullptr;
    }

    alloc_slice DocumentMeta::encode() const {
        Encoder enc;
        enc.beginArray(3);
        enc << (unsigned)flags;
        enc << version;
        enc << docType;
        enc.endArray();
        return enc.extractOutput();
    }

    alloc_slice DocumentMeta::encodeAndUpdate() {
        auto bytes = encode();
        decode(bytes);
        return bytes;
    }

}
