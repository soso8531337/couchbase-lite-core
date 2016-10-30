//
//  LiteCoreObjCTests.m
//  LiteCoreObjCTests
//
//  Created by Jens Alfke on 10/13/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#import <XCTest/XCTest.h>
#import "LCDatabase.h"
#import "LCDocument.h"
#import "c4Base.h"


#define Assert XCTAssert
#define AssertNotNil XCTAssertNotNil
#define AssertEqual XCTAssertEqual
#define AssertEqualObjects XCTAssertEqualObjects
#define AssertFalse XCTAssertFalse


// Internal API; I use this to create multiple doc instances on a single document
@interface LCDocument ()
- (instancetype) initWithDatabase: (LCDatabase*)db
                            docID: (NSString*)docID
                        mustExist: (BOOL)mustExist
                            error: (NSError**)outError;
@end


@interface LiteCoreObjCTests : XCTestCase
@end


@implementation LiteCoreObjCTests
{
    LCDatabase* db;
}


- (void)setUp {
    [super setUp];
    // (Can't use default directory because XCTest doesn't have a bundle ID)
    NSString* path = [NSTemporaryDirectory() stringByAppendingPathComponent: @"LCDatabaseTest"];
    NSError* error;
    Assert([LCDatabase deleteDatabaseAtPath: path error: &error]);
    db = [[LCDatabase alloc] initWithPath: path error: &error];
    AssertNotNil(db, @"Couldn't open db: %@", error);
}


- (void)tearDown {
    NSError *error;
    Assert([db close: &error]);
    db = nil;
    [super tearDown];
}


- (void)test01_NewDoc {
    // Create a new document:
    LCDocument* doc = db[@"doc1"];
    AssertNotNil(doc);
    AssertEqualObjects(doc.documentID, @"doc1");
    AssertEqual(doc.database, db);
    AssertEqual(doc.sequence, 0);
    AssertFalse(doc.exists);
    AssertFalse(doc.isDeleted);
    AssertEqual(doc.properties, nil);
    AssertEqual(doc[@"prop"], nil);
    AssertFalse([doc boolForKey: @"prop"]);
    AssertEqual([doc integerForKey: @"prop"], 0);
    AssertEqual([doc floatForKey: @"prop"], 0.0);
    AssertEqual([doc doubleForKey: @"prop"], 0.0);
    AssertFalse(doc.hasUnsavedChanges);
    AssertEqual(doc.savedProperties, nil);

    // Try and fail to load:
    NSError* error;
    AssertFalse([doc reload: &error]);
    AssertEqualObjects(error.domain, LCErrorDomain);
    AssertEqual(error.code, kC4ErrorNotFound);

    AssertEqual(db[@"doc1"], doc);

    AssertEqual([db documentWithID: @"doc1" mustExist: true error: &error], nil);
    AssertEqualObjects(error.domain, LCErrorDomain);
    AssertEqual(error.code, kC4ErrorNotFound);
    AssertEqual([db documentWithID: @"doc2" mustExist: true error: NULL], nil);
    AssertEqualObjects(error.domain, LCErrorDomain);
    AssertEqual(error.code, kC4ErrorNotFound);
}


- (void)test02_SetProperties {
    LCDocument* doc = db[@"doc1"];
    doc[@"type"] = @"demo";
    doc[@"weight"] = @12.5;
    doc[@"tags"] = @[@"useless", @"temporary"];

    Assert(doc.hasUnsavedChanges);
    AssertEqualObjects(doc[@"type"], @"demo");
    AssertEqual([doc doubleForKey: @"weight"], 12.5);
    AssertEqualObjects(doc.properties,
                       (@{@"type": @"demo", @"weight": @12.5, @"tags": @[@"useless", @"temporary"]}));
    [doc revertToSaved];
    Assert(!doc.hasUnsavedChanges);
    AssertEqualObjects(doc[@"type"], nil);
    AssertEqual([doc doubleForKey: @"weight"], 0);
    AssertEqualObjects(doc.properties, nil);
}


- (void)test03_SaveNewDoc {
    LCDocument* doc = db[@"doc1"];
    doc[@"type"] = @"demo";
    doc[@"weight"] = @12.5;
    doc[@"tags"] = @[@"useless", @"temporary"];
    NSError *error;
    Assert([doc save: &error], @"Error saving: %@", error);

    Assert(!doc.hasUnsavedChanges);
    AssertEqualObjects(doc[@"type"], @"demo");
    AssertEqualObjects(doc.properties,
                       (@{@"type": @"demo", @"weight": @12.5, @"tags": @[@"useless", @"temporary"]}));
    AssertEqual(doc.sequence, 1);

    // Create a different LCDocument on the same doc, so we can read it back in:
    LCDocument* firstDoc = doc;
    doc = [[LCDocument alloc] initWithDatabase: db docID: @"doc1" mustExist: YES error: NULL];
    Assert(doc != firstDoc);

    AssertEqual(doc.sequence, 1);
    Assert(!doc.hasUnsavedChanges);
    AssertEqualObjects(doc[@"type"], @"demo");
    AssertEqual([doc doubleForKey: @"weight"], 12.5);
    AssertEqualObjects(doc.properties,
                       (@{@"type": @"demo", @"weight": @12.5, @"tags": @[@"useless", @"temporary"]}));
}


- (void) test04_Conflict {
    LCDocument* doc = db[@"doc1"];
    doc[@"type"] = @"demo";
    doc[@"weight"] = @12.5;
    doc[@"tags"] = @[@"useless", @"temporary"];
    NSError *error;
    Assert([doc save: &error], @"Error saving: %@", error);

    doc[@"weight"] = @200;

    // Update the doc in the db, by using a different object:
    {
        LCDocument *shadow = [[LCDocument alloc] initWithDatabase: db docID: @"doc1"
                                                        mustExist: YES error: NULL];
        AssertEqualObjects(shadow[@"weight"], @12.5);
        shadow[@"weight"] = @1.5;
        Assert([shadow save: &error], @"Error saving shadow: %@", error);
    }

    AssertFalse([doc save: &error]);
    AssertEqualObjects(error.domain, LCErrorDomain);
    AssertEqual(error.code, kC4ErrorConflict);

    // Conflict resolver that returns nil aborts the resolve:
    doc.conflictResolver = ^(NSDictionary *mine, NSDictionary* theirs, NSDictionary* base) {
        return (NSDictionary*)nil;
    };
    AssertFalse([doc save: &error]);

    // Now use a resolver that just takes my changes:
    doc.conflictResolver = ^(NSDictionary *mine, NSDictionary* theirs, NSDictionary* base) {
        AssertEqualObjects(mine[@"weight"], @200);
        AssertEqualObjects(theirs[@"weight"], @1.5);
        AssertEqualObjects(base[@"weight"], @12.5);
        return mine;
    };
    Assert([doc save: &error], @"Error updating: %@", error);
    AssertEqualObjects(doc[@"weight"], @200);
}

@end