//
//  cbliteTool+file.cc
//  LiteCore
//
//  Created by Jens Alfke on 9/8/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "cbliteTool.hh"


void CBLiteTool::fileUsage() {
    writeUsageCommand("file", false);
    cerr <<
    "  Displays information about the database\n"
    ;
}


void CBLiteTool::fileInfo() {
    // Read params:
    processFlags(nullptr);
    if (_showHelp) {
        fileUsage();
        return;
    }
    openDatabaseFromNextArg();
    endOfArgs();

    alloc_slice pathSlice = c4db_getPath(_db);
    auto nDocs = c4db_getDocumentCount(_db);
    auto lastSeq = c4db_getLastSequence(_db);
    alloc_slice indexesFleece = c4db_getIndexes(_db, nullptr);
    auto indexes = Value::fromData(indexesFleece).asArray();

    FilePath path(pathSlice.asString());
    uint64_t dbSize = 0, blobsSize = 0, nBlobs = 0;
    path["db.sqlite3"].forEachMatch([&](const litecore::FilePath &file) {
        dbSize += file.dataSize();
    });
    auto attachmentsPath = path["Attachments/"];
    if (attachmentsPath.exists()) {
        attachmentsPath.forEachFile([&](const litecore::FilePath &file) {
            blobsSize += file.dataSize();
        });
    }

    cout << "Database:   " << pathSlice << "\n";
    cout << "Total size: "; writeSize(dbSize + blobsSize); cerr << "\n";
    cout << "Documents:  " << nDocs << ", last sequence " << lastSeq << "\n";

    if (indexes.count() > 0) {
        cout << "Indexes:    ";
        int n = 0;
        for (Array::iterator i(indexes); i; ++i) {
            if (n++)
                cout << ", ";
            cout << i.value().asString();
        }
        cout << "\n";
    }

    if (nBlobs > 0) {
        cout << "Blobs:      " << nBlobs << ", "; writeSize(dbSize); cerr << "\n";
    }

    C4UUID publicUUID, privateUUID;
    if (c4db_getUUIDs(_db, &publicUUID, &privateUUID, nullptr)) {
        cout << "UUIDs:      public "
             << slice(&publicUUID, sizeof(publicUUID)).hexString().c_str()
             << ", private " << slice(&privateUUID, sizeof(privateUUID)).hexString().c_str()
             << "\n";
    }
}


void CBLiteTool::writeSize(uint64_t n) {
    static const char* kScales[] = {" bytes", "KB", "MB", "GB"};
    int scale = 0;
    while (n >= 1024 && scale < 3) {
        n = (n + 512) / 1024;
        ++scale;
    }
    cout << n << kScales[scale];
}
