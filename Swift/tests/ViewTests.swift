//
//  ViewTests.swift
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 4/13/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

import Foundation
@testable import LiteCoreSwift


class ViewTests: SwiftForestTestCase {

    var view: View!

    static let kPath = "/tmp/swiftforest.viewdb"

    override func setUp() {
        super.setUp()

        try! View.delete(ViewTests.kPath)
        view = try! View(database: db,
                         path: ViewTests.kPath,
                         name: "test",
                         version: "1") {
                            doc, emit in
                            print("map(\(doc))")
                            emit(doc["i"]!, nil) }
    }

    override func tearDown() {
        view = nil
        super.tearDown()
    }


    func test01_Index() throws {
        createNumberedDocs(1...100)
        try ViewIndexer(views:[view]).run()
        check(view.totalRows) == 100
    }

    func test02_BasicQuery() throws {
        createNumberedDocs(1...100)
        try ViewIndexer(views:[view]).run()

        var i = 0
        for row in Query(view) {
            i += 1
            //print("doc = \(row.docID)   key = \(row.key)   value = \(row.value)")
            check(row.key) == Val.int(i)
            check(row.value).isNil()
            check(row.docID) == String(format: "doc-%03d", i)
        }
        check(i) == 100
    }
}
