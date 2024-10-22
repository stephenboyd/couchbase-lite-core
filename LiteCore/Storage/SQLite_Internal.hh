//
//  SQLite_Internal.hh
//  LiteCore
//
//  Created by Jens Alfke on 9/28/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#pragma once
#include "DataFile.hh"
#include "Logging.hh"
#include <memory>

struct sqlite3;

namespace SQLite {
    class Database;
    class Statement;
    class Transaction;
}
namespace fleece {
    class SharedKeys;
}


namespace litecore {

    extern LogDomain SQL;

    void LogStatement(const SQLite::Statement &st);


    // Little helper class that makes sure Statement objects get reset on exit
    class UsingStatement {
    public:
        UsingStatement(SQLite::Statement &stmt) noexcept;

        UsingStatement(const std::unique_ptr<SQLite::Statement> &stmt) noexcept
        :UsingStatement(*stmt.get())
        { }

        ~UsingStatement();

    private:
        SQLite::Statement &_stmt;
    };


    void RegisterSQLiteFunctions(sqlite3 *db,
                                 DataFile::FleeceAccessor accessor,
                                 fleece::SharedKeys *sharedKeys);
}
