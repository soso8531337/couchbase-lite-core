excluded = ["c4log", "c4error_getMessageC", "c4str"]
force_no_bridge = ["c4SliceEqual"]
default_param_name = {"C4SliceResult":"slice","C4WriteStream*":"stream","C4ReadStream*":"stream","C4Error*":"outError",
"C4BlobStore*":"store","C4BlobKey":"key","C4Slice":"slice","C4Key*":"key","bool":"b","double":"d","C4KeyReader*":"reader",
"C4DatabaseObserver*":"observer","C4DocumentObserver*":"observer","C4View*":"view","C4OnCompactCallback":"callback",
"C4Database*":"db","C4SequenceNumber":"sequence","C4StringResult":"str","C4String":"str"}
param_bridge_types = ["C4Slice", "size_t", "size_t*","C4Slice[]", "C4String", "C4String[]"]
return_bridge_types = ["C4SliceResult", "C4Slice", "size_t", "byte*", "C4StringResult", "C4String"]
type_map = {"int32_t":"int","uint32_t":"uint","int64_t":"long","uint64_t":"ulong","size_t":"UIntPtr","size_t*":"UIntPtr*","C4SequenceNumber":"ulong","C4SequenceNumber*":"ulong*","unsigned":"uint","FLSharedKeys":"FLSharedKeys*","char*":"byte*"}
reserved = ["string"]
