//
//  LiteCoreTest.hh
//  Couchbase Lite Core
//
//  Created by Jens Alfke on 5/24/16.
//  Copyright (c) 2016 Couchbase. All rights reserved.
//

#pragma once

#include <iostream>
#include "slice.hh"
#include "PlatformCompat.hh"
#include "Logging.hh"

#ifdef DEBUG
#define CHECK_IF_DEBUG CHECK
#define REQUIRE_IF_DEBUG REQUIRE
#else
#define CHECK_IF_DEBUG(x)
#define REQUIRE_IF_DEBUG(x)
#endif

using namespace fleece;


std::string stringWithFormat(const char *format, ...) __printflike(1, 2);

// Converts back-quotes to double-quotes; useful for JSON strings
std::string enquotify(std::string);

std::string sliceToHex(slice);
std::string sliceToHexDump(slice, size_t width = 16);

void randomBytes(slice dst);

// Some operators to make slice work with unit-testing assertions:
// (This has to be declared before including catch.hpp, because C++ sucks)
namespace fleece {
    std::ostream& operator<< (std::ostream& o, slice s);
}


#include "CatchHelper.hh"

#include "DataFile.hh"

using namespace litecore;
using namespace std;


class DataFileTestFixture {
public:

    static const int numberOfOptions = 2;

    DataFileTestFixture()   :DataFileTestFixture(0) { }     // defaults to SQLite, rev-trees
    DataFileTestFixture(int testOption);
    ~DataFileTestFixture();

    DataFile::Factory& factory();
    
    DataFile *db {nullptr};
    KeyStore *store {nullptr};

    FilePath databasePath(const string baseName);
    void deleteDatabase(const FilePath &dbPath);
    DataFile* newDatabase(const FilePath &path, DataFile::Options* =nullptr);
    void reopenDatabase(DataFile::Options *newOptions =nullptr);


};

