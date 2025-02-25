//
//  cbliteTool.hh
//  LiteCore
//
//  Created by Jens Alfke on 9/8/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#pragma once
#include "Tool.hh"
#include "c4Document+Fleece.h"
#include "FilePath.hh"
#include "StringUtil.hh"
#include <exception>
#include <fnmatch.h>        // POSIX (?)
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <vector>

using namespace std;
using namespace fleeceapi;


class CBLiteTool : public Tool {
public:
    CBLiteTool() {
    }

    virtual ~CBLiteTool() {
        c4db_free(_db);
    }

    // Main handlers:
    void usage() override;
    int run() override;

private:

    void openDatabase(string path);
    void openDatabaseFromNextArg();

    // Shell command
    void shell();
    void runInteractively();
    void helpCommand();
    void quitCommand();

    // Query command
    void queryUsage();
    void queryDatabase();
    alloc_slice convertQuery(slice inputQuery);

    // ls command
    void listUsage();
    void listDocsCommand();
    void listDocs(string docIDPattern);

    // cat command
    void catUsage();
    void catDocs();
    void catDoc(C4Document *doc, bool includeID);

    // file command
    void fileUsage();
    void fileInfo();

    // revs command
    void revsUsage();
    void revsInfo();

    // sql command
    void sqlUsage();
    void sqlQuery();

    using RevTree = map<alloc_slice,set<alloc_slice>>; // Maps revID to set of child revIDs

    void writeRevisionTree(C4Document *doc,
                           RevTree &tree,
                           alloc_slice root,
                           const string &indent);
    void writeRevisionChildren(C4Document *doc,
                               RevTree &tree,
                               alloc_slice root,
                               const string &indent);

#pragma mark - UTILITIES:


    static void writeSize(uint64_t n);

    void writeUsageCommand(const char *cmd, bool hasFlags, const char *otherArgs ="");

    c4::ref<C4Document> readDoc(string docID);

    void rawPrint(Value body, slice docID, slice revID =nullslice);
    void prettyPrint(Value value,
                     const string &indent ="",
                     slice docID =nullslice,
                     slice revID =nullslice,
                     const std::set<alloc_slice> *onlyKeys =nullptr);

    static bool canBeUnquotedJSON5Key(slice key);

    static bool isGlobPattern(string &str);
    static void unquoteGlobPattern(string &str);


#pragma mark - FLAGS:


    void clearFlags() {
        _offset = 0;
        _limit = -1;
        _startKey = _endKey = nullslice;
        _keys.clear();
        _enumFlags = kC4IncludeNonConflicted;
        _longListing = _listBySeq = false;
        _showRevID = false;
        _prettyPrint = true;
        _json5 = false;
        _showHelp = false;
    }


    void offsetFlag()    {_offset = stoul(nextArg("offset value"));}
    void limitFlag()     {_limit = stol(nextArg("limit value"));}
    void keyFlag()       {_keys.insert(alloc_slice(nextArg("key")));}
    void longListFlag()  {_longListing = true;}
    void seqFlag()       {_listBySeq = true;}
    void bodyFlag()      {_enumFlags |= kC4IncludeBodies;}
    void descFlag()      {_enumFlags |= kC4Descending;}
    void delFlag()       {_enumFlags |= kC4IncludeDeleted;}
    void confFlag()      {_enumFlags &= ~kC4IncludeNonConflicted;}
    void revIDFlag()     {_showRevID = true;}
    void prettyFlag()    {_prettyPrint = true; _enumFlags |= kC4IncludeBodies;}
    void json5Flag()     {_json5 = true; _enumFlags |= kC4IncludeBodies;}
    void rawFlag()       {_prettyPrint = false; _enumFlags |= kC4IncludeBodies;}
    void helpFlag()      {_showHelp = true;}


    static const FlagSpec kSubcommands[];
    static const FlagSpec kInteractiveSubcommands[];
    static const FlagSpec kQueryFlags[];
    static const FlagSpec kListFlags[];
    static const FlagSpec kCatFlags[];


    C4Database* _db {nullptr};
    bool _interactive {false};
    uint64_t _offset {0};
    int64_t _limit {-1};
    alloc_slice _startKey, _endKey;
    std::set<alloc_slice> _keys;
    C4EnumeratorFlags _enumFlags {kC4IncludeNonConflicted};
    bool _longListing {false};
    bool _listBySeq {false};
    bool _prettyPrint {true};
    bool _json5 {false};
    bool _showRevID {false};
    bool _showHelp {false};
};
