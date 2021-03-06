using System;
using System.IO;
using System.Threading.Tasks;
using FluentAssertions;
using LiteCore.Interop;
using Xunit;

namespace LiteCore.Tests
{
    public unsafe class DatabaseTest : Test
    {
        [Fact]
        public void TestErrorMessages()
        {
            var msg = Native.c4error_getMessage(new C4Error(C4ErrorDomain.ForestDBDomain, 0));
            msg.Should().BeNull("because there was no error");

            AssertMessage(C4ErrorDomain.SQLiteDomain, (int)SQLiteStatus.Corrupt, "database disk image is malformed");
            AssertMessage(C4ErrorDomain.LiteCoreDomain, (int)LiteCoreError.InvalidParameter, "invalid parameter");
            AssertMessage(C4ErrorDomain.POSIXDomain, (int)PosixStatus.NOENT, "No such file or directory");
            AssertMessage(C4ErrorDomain.LiteCoreDomain, (int)LiteCoreError.IndexBusy, "index busy; can't close view");
            AssertMessage((C4ErrorDomain)666, -1234, "unknown error domain");
        }

        [Fact]
        public void TestDatabaseInfo()
        {
            RunTestVariants(() => {
                Native.c4db_getDocumentCount(Db).Should().Be(0, "because the database is empty");
                Native.c4db_getLastSequence(Db).Should().Be(0, "because the database is empty");
                var publicID = new C4UUID();
                var privateID = new C4UUID();
                LiteCoreBridge.Check(err => {
                    var publicID_ = publicID;
                    var privateID_ = privateID;
                    var retVal = Native.c4db_getUUIDs(Db, &publicID_, &privateID_, err);
                    publicID = publicID_;
                    privateID = privateID_;
                    return retVal;
                });

                // Odd quirk of C# means we need an additional copy
                var p1 = publicID;
                var p2 = privateID;
                publicID.Should().NotBe(privateID, "because public UUID and private UUID should differ");
                (p1.bytes[6] & 0xF0).Should().Be(0x40, "because otherwise the UUID is non-conformant");
                (p1.bytes[8] & 0xC0).Should().Be(0x80, "because otherwise the UUID is non-conformant");
                (p2.bytes[6] & 0xF0).Should().Be(0x40, "because otherwise the UUID is non-conformant");
                (p2.bytes[8] & 0xC0).Should().Be(0x80, "because otherwise the UUID is non-conformant");

                // Make sure the UUIDs are persistent
                ReopenDB();
                var publicID2 = new C4UUID();
                var privateID2 = new C4UUID();
                LiteCoreBridge.Check(err => {
                    var publicID_ = publicID2;
                    var privateID_ = privateID2;
                    var retVal = Native.c4db_getUUIDs(Db, &publicID_, &privateID_, err);
                    publicID2 = publicID_;
                    privateID2 = privateID_;
                    return retVal;
                });

                publicID2.Should().Be(publicID, "because the public UUID should persist");
                privateID2.Should().Be(privateID, "because the private UUID should persist");
            });
        }

        [Fact]
        public void TestOpenBundle()
        {
            RunTestVariants(() => {
                var config = C4DatabaseConfig.Clone(Native.c4db_getConfig(Db));
                config.flags |= C4DatabaseFlags.Bundled;
                var tmp = config;

                var bundlePath = Path.Combine(TestDir, $"cbl_core_test_bundle{Path.DirectorySeparatorChar}");
                Native.c4db_deleteAtPath(bundlePath, &config, null);
                var bundle = (C4Database *)LiteCoreBridge.Check(err => {
                    var localConfig = tmp;
                    return Native.c4db_open(bundlePath, &localConfig, err);
                });

                var path = Native.c4db_getPath(bundle);
                path.Should().Be(bundlePath, "because the database should store the correct path");
                LiteCoreBridge.Check(err => Native.c4db_close(bundle, err));
                Native.c4db_free(bundle);

                // Reopen without the 'create' flag:
                config.flags &= ~C4DatabaseFlags.Create;
                tmp = config;
                bundle = (C4Database *)LiteCoreBridge.Check(err => {
                    var localConfig = tmp;
                    return Native.c4db_open(bundlePath, &localConfig, err);
                });
                LiteCoreBridge.Check(err => Native.c4db_close(bundle, err));
                Native.c4db_free(bundle);

                // Reopen with wrong storage type:
                NativePrivate.c4log_warnOnErrors(false);
                var engine = config.storageEngine;
                config.storageEngine = "b0gus";
                ((long)Native.c4db_open(bundlePath, &config, null)).Should().Be(0, "because the storage engine is nonsense");
                config.storageEngine = engine;

                // Open nonexistent bundle
                ((long)Native.c4db_open($"no_such_bundle{Path.DirectorySeparatorChar}", &config, null)).Should().Be(0, "because the bundle does not exist");
                NativePrivate.c4log_warnOnErrors(true);

                config.Dispose();
            });
        }

        [Fact]
        public void TestTransaction()
        {
            RunTestVariants(() => {
                Native.c4db_getDocumentCount(Db).Should().Be(0, "because no documents have been added");
                Native.c4db_isInTransaction(Db).Should().BeFalse("because no transaction has started yet");
                LiteCoreBridge.Check(err => Native.c4db_beginTransaction(Db, err));
                Native.c4db_isInTransaction(Db).Should().BeTrue("because a transaction has started");
                LiteCoreBridge.Check(err => Native.c4db_beginTransaction(Db, err));
                Native.c4db_isInTransaction(Db).Should().BeTrue("because another transaction has started");
                LiteCoreBridge.Check(err => Native.c4db_endTransaction(Db, true, err));
                Native.c4db_isInTransaction(Db).Should().BeTrue("because a transaction is still active");
                LiteCoreBridge.Check(err => Native.c4db_endTransaction(Db, true, err));
                Native.c4db_isInTransaction(Db).Should().BeFalse("because all transactions have ended");
            });
        }

        [Fact]
        public void TestCreateRawDoc()
        {
            RunTestVariants(() => {
                var key = C4Slice.Constant("key");
                var meta = C4Slice.Constant("meta");
                LiteCoreBridge.Check(err => Native.c4db_beginTransaction(Db, err));
                LiteCoreBridge.Check(err => NativeRaw.c4raw_put(Db, C4Slice.Constant("test"), key, meta, 
                    Body, err));
                LiteCoreBridge.Check(err => Native.c4db_endTransaction(Db, true, err));

                var doc = (C4RawDocument *)LiteCoreBridge.Check(err => NativeRaw.c4raw_get(Db,
                    C4Slice.Constant("test"), key, err));
                doc->key.Equals(key).Should().BeTrue("because the key should not change");
                doc->meta.Equals(meta).Should().BeTrue("because the meta should not change");
                doc->body.Equals(Body).Should().BeTrue("because the body should not change");
                Native.c4raw_free(doc);

                // Nonexistent:
                C4Error error;
                ((long)Native.c4raw_get(Db, "test", "bogus", &error)).Should().Be(0, 
                    "because the document does not exist");
                error.domain.Should().Be(C4ErrorDomain.LiteCoreDomain, "because that is the correct domain");
                error.code.Should().Be((int)LiteCoreError.NotFound, "because that is the correct error code");
            });
        }

        [Fact]
        public void TestAllDocs()
        {
            RunTestVariants(() => {
                SetupAllDocs();

                Native.c4db_getDocumentCount(Db).Should().Be(99UL, "because there are 99 non-deleted documents");

                // No start or end ID:
                var options = C4EnumeratorOptions.Default;
                options.flags &= ~C4EnumeratorFlags.IncludeBodies;
                var e = (C4DocEnumerator *)LiteCoreBridge.Check(err => {
                    var localOpts = options;
                    return Native.c4db_enumerateAllDocs(Db, null, null, &localOpts, err);
                });

                int i = 1;
                C4Error error;
                while(Native.c4enum_next(e, &error)) {
                    var doc = (C4Document *)LiteCoreBridge.Check(err => Native.c4enum_getDocument(e, err));
                    var docID = $"doc-{i:D3}";
                    doc->docID.CreateString().Should().Be(docID, "because the doc should have the correct doc ID");
                    doc->revID.Equals(RevID).Should().BeTrue("because the doc should have the current revID");
                    doc->selectedRev.revID.Equals(RevID).Should().BeTrue("because the selected rev should have the correct rev ID");
                    doc->selectedRev.sequence.Should().Be((ulong)i, "because the sequences should come in order");
                    doc->selectedRev.body.Equals(C4Slice.Null).Should().BeTrue("because the body is not loaded yet");
                    LiteCoreBridge.Check(err => Native.c4doc_loadRevisionBody(doc, err));
                    doc->selectedRev.body.Equals(Body).Should().BeTrue("because the loaded body should be correct");

                    C4DocumentInfo info;
                    Native.c4enum_getDocumentInfo(e, &info).Should().BeTrue("because otherwise the doc info load failed");
                    info.docID.CreateString().Should().Be(docID, "because the doc info should have the correct doc ID");
                    info.revID.Equals(RevID).Should().BeTrue("because the doc info should have the correct rev ID");

                    Native.c4doc_free(doc);
                    i++;
                }

                Native.c4enum_free(e);
                i.Should().Be(100);

                // Start and end ID:
                e = (C4DocEnumerator *)LiteCoreBridge.Check(err => Native.c4db_enumerateAllDocs(Db, 
                    "doc-007", "doc-090", null, err));
                i = 7;
                while(Native.c4enum_next(e, &error)) {
                    error.code.Should().Be(0, "because otherwise an enumeration error occurred");
                    var doc = (C4Document *)LiteCoreBridge.Check(err => Native.c4enum_getDocument(e, err));
                    var docID = $"doc-{i:D3}";
                    doc->docID.CreateString().Should().Be(docID, "because the doc should have the correct doc ID");
                    Native.c4doc_free(doc);
                    i++;
                }

                Native.c4enum_free(e);
                i.Should().Be(91, "because that is how many documents fall in the given range");

                // Some docs, by ID:
                options = C4EnumeratorOptions.Default;
                options.flags |= C4EnumeratorFlags.IncludeDeleted;
                var docIDs = new[] { "doc-042", "doc-007", "bogus", "doc-001" };
                e = (C4DocEnumerator *)LiteCoreBridge.Check(err => {
                    var localOpts = options;
                    return Native.c4db_enumerateSomeDocs(Db, docIDs, &localOpts, err);
                });

                i = 0;
                while(Native.c4enum_next(e, &error)) {
                    error.code.Should().Be(0, "because otherwise an enumeration error occurred");
                    var doc = (C4Document *)LiteCoreBridge.Check(err => Native.c4enum_getDocument(e, err));
                    doc->docID.CreateString().Should().Be(docIDs[i], "because the doc should have the correct sorted doc ID");
                    if(doc->sequence != 0) {
                        i.Should().NotBe(2, "because no document exists with the 'bogus' key");
                    }

                    Native.c4doc_free(doc);
                    i++;
                }

                Native.c4enum_free(e);
                i.Should().Be(4, "because four document IDs were specified");
            });
        }

        [Fact]
        public void TestAllDocsIncludeDeleted()
        {
            RunTestVariants(() => {
                SetupAllDocs();
                var options = C4EnumeratorOptions.Default;
                options.flags |= C4EnumeratorFlags.IncludeDeleted;
                var e = (C4DocEnumerator *)LiteCoreBridge.Check(err => {
                    var localOpts = options;
                    return Native.c4db_enumerateAllDocs(Db, "doc-004", "doc-007", &localOpts, err);
                });

                int i = 4;
                C4Error error;
                while(Native.c4enum_next(e, &error)) {
                    var doc = (C4Document *)LiteCoreBridge.Check(err => Native.c4enum_getDocument(e, err));
                    var docID = default(string);
                    if(i == 6) {
                        docID = "doc-005DEL";
                    } else {
                        var docNum = i >= 6 ? i - 1 : i;
                        docID = $"doc-{docNum:D3}";
                    }

                    doc->docID.CreateString().Should().Be(docID, "because the doc should have the correct doc ID");
                    Native.c4doc_free(doc);
                    i++;
                }

                Native.c4enum_free(e);
                i.Should().Be(9, "because that is the last ID suffix in the given range");
            });
        }

        [Fact]
        public void TestAllDocsInfo()
        {
            RunTestVariants(() => {
                SetupAllDocs();

                var options = C4EnumeratorOptions.Default;
                var e = (C4DocEnumerator *)LiteCoreBridge.Check(err => {
                    var localOpts = options;
                    return Native.c4db_enumerateAllDocs(Db, null, null, &localOpts, err);
                });

                int i = 1;
                C4Error error;
                while(Native.c4enum_next(e, &error)) {
                    C4DocumentInfo doc;
                    Native.c4enum_getDocumentInfo(e, &doc).Should().BeTrue("because otherwise getting the doc info failed");
                    var docID = $"doc-{i:D3}";
                    doc.docID.CreateString().Should().Be(docID, "because the doc info should have the correct doc ID");
                    doc.revID.Equals(RevID).Should().BeTrue("because the doc info should have the correct rev ID");
                    doc.sequence.Should().Be((ulong)i, "because the doc info should have the correct sequence");
                    doc.flags.Should().Be(C4DocumentFlags.Exists, "because the doc info should have the correct flags");
                    i++;
                }

                Native.c4enum_free(e);
                error.code.Should().Be(0, "because otherwise an error occurred somewhere");
                i.Should().Be(100, "because all docs should be iterated, even deleted ones");
            });
        }

        [Fact]
        public void TestChanges()
        {
            RunTestVariants(() => {
                for(int i = 1; i < 100; i++) {
                    var docID = $"doc-{i:D3}";
                    CreateRev(docID, RevID, Body);
                }

                // Since start:
                var options = C4EnumeratorOptions.Default;
                options.flags &= ~C4EnumeratorFlags.IncludeBodies;
                var e = (C4DocEnumerator *)LiteCoreBridge.Check(err => {
                    var localOpts = options;
                    return Native.c4db_enumerateChanges(Db, 0, &localOpts, err);
                });

                var seq = 1UL;
                C4Document* doc;
                C4Error error;
                while(null != (doc = Native.c4enum_nextDocument(e, &error))) {
                    doc->selectedRev.sequence.Should().Be(seq, "because the sequence numbers should be ascending");
                    var docID = $"doc-{seq:D3}";
                    doc->docID.CreateString().Should().Be(docID, "because the doc should have the correct doc ID");
                    Native.c4doc_free(doc);
                    seq++;
                }

                Native.c4enum_free(e);

                // Since 6:
                e = (C4DocEnumerator *)LiteCoreBridge.Check(err => {
                    var localOpts = options;
                    return Native.c4db_enumerateChanges(Db, 6, &localOpts, err);
                });

                seq = 7;
                while(null != (doc = Native.c4enum_nextDocument(e, &error))) {
                    doc->selectedRev.sequence.Should().Be(seq, "because the sequence numbers should be ascending");
                    var docID = $"doc-{seq:D3}";
                    doc->docID.CreateString().Should().Be(docID, "because the doc should have the correct doc ID");
                    Native.c4doc_free(doc);
                    seq++;
                }

                Native.c4enum_free(e);
                seq.Should().Be(100UL, "because that is the highest sequence in the DB");
            });
        }

        [Fact]
        public void TestExpired()
        {
            RunTestVariants(() => {
                const string docID = "expire_me";
                CreateRev(docID, RevID, Body);
                var expire = DateTimeOffset.UtcNow.Add(TimeSpan.FromSeconds(1)).ToUnixTimeSeconds();
                LiteCoreBridge.Check(err => Native.c4doc_setExpiration(Db, docID, (ulong)expire, err));

                expire = DateTimeOffset.UtcNow.Add(TimeSpan.FromSeconds(2)).ToUnixTimeSeconds();
                // Make sure setting it to the same is also true
                LiteCoreBridge.Check(err => Native.c4doc_setExpiration(Db, docID, (ulong)expire, err));
                LiteCoreBridge.Check(err => Native.c4doc_setExpiration(Db, docID, (ulong)expire, err));

                const string docID2 = "expire_me_too";
                CreateRev(docID2, RevID, Body);
                LiteCoreBridge.Check(err => Native.c4doc_setExpiration(Db, docID2, (ulong)expire, err));

                const string docID3 = "dont_expire_me";
                CreateRev(docID3, RevID, Body);
                Task.Delay(TimeSpan.FromSeconds(2)).Wait();

                var e = (C4ExpiryEnumerator *)LiteCoreBridge.Check(err => Native.c4db_enumerateExpired(Db, err));
                int expiredCount = 0;
                while(Native.c4exp_next(e, null)) {
                    var existingDocID = Native.c4exp_getDocID(e);
                    existingDocID.Should().NotBe(docID3, "because the last document is not scheduled for expiration");
                    expiredCount++;
                }

                Native.c4exp_free(e);
                expiredCount.Should().Be(2, "because 2 documents were scheduled for expiration");
                Native.c4doc_getExpiration(Db, docID).Should().Be((ulong)expire, "because that was what was set as the expiration");
                Native.c4doc_getExpiration(Db, docID2).Should().Be((ulong)expire, "because that was what was set as the expiration");
                Native.c4db_nextDocExpiration(Db).Should().Be((ulong)expire, "because that is the closest expiration date");

                e = (C4ExpiryEnumerator *)LiteCoreBridge.Check(err => Native.c4db_enumerateExpired(Db, err));
                expiredCount = 0;
                while(Native.c4exp_next(e, null)) {
                    var existingDocID = Native.c4exp_getDocID(e);
                    existingDocID.Should().NotBe(docID3, "because the last document is not scheduled for expiration");
                    expiredCount++;
                }

                LiteCoreBridge.Check(err => Native.c4exp_purgeExpired(e, err));
                Native.c4exp_free(e);
                expiredCount.Should().Be(2, "because 2 documents were scheduled for expiration");
                
                e = (C4ExpiryEnumerator *)LiteCoreBridge.Check(err => Native.c4db_enumerateExpired(Db, err));
                expiredCount = 0;
                while(Native.c4exp_next(e, null)) {
                    expiredCount++;
                }

                LiteCoreBridge.Check(err => Native.c4exp_purgeExpired(e, err));
                Native.c4exp_free(e);
                expiredCount.Should().Be(0, "because no more documents were scheduled for expiration");
            });
        }

        [Fact]
        public void TestCancelExpire()
        {
            RunTestVariants(() => {
                const string docID = "expire_me";
                CreateRev(docID, RevID, Body);
                var expire = (ulong)DateTimeOffset.UtcNow.AddSeconds(2).ToUnixTimeSeconds();
                LiteCoreBridge.Check(err => Native.c4doc_setExpiration(Db, docID, expire, err));
                LiteCoreBridge.Check(err => Native.c4doc_setExpiration(Db, docID, UInt64.MaxValue, err));

                Task.Delay(TimeSpan.FromSeconds(2)).Wait();
                var e = (C4ExpiryEnumerator *)LiteCoreBridge.Check(err => Native.c4db_enumerateExpired(Db, err));

                int expiredCount = 0;
                while(Native.c4exp_next(e, null)) {
                    expiredCount++;
                }

                LiteCoreBridge.Check(err => Native.c4exp_purgeExpired(e, err));
                Native.c4exp_free(e);
                expiredCount.Should().Be(0, "because the expiration was cancelled");    
            });
        }

        [Fact]
        public void TestDatabaseBlobStore()
        {
            RunTestVariants(() => {
                LiteCoreBridge.Check(err => Native.c4db_getBlobStore(Db, err));
            });
        }

        private void SetupAllDocs()
        {
            for(int i = 1; i < 100; i++) {
                var docID = $"doc-{i:D3}";
                CreateRev(docID, RevID, Body);
            }

            // Add a deleted doc to make sure it's skipped by default:
            CreateRev("doc-005DEL", RevID, C4Slice.Null, C4RevisionFlags.Deleted);
        }

        private void AssertMessage(C4ErrorDomain domain, int code, string expected)
        {
            var msg = Native.c4error_getMessage(new C4Error(domain, code));
            msg.Should().Be(expected, "because the error message should match the code");
        }
    }
}