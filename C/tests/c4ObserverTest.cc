//
//  c4ObserverTest.cc
//  LiteCore
//
//  Created by Jens Alfke on 11/7/16.
//  Copyright © 2016 Couchbase. All rights reserved.
//

#include "c4Test.hh"
#include "c4Observer.h"


class C4ObserverTest : public C4Test {
    public:

    // This test is not dependent on different storage/versioning types.
    C4ObserverTest() :C4Test(0) { }

    ~C4ObserverTest() {
        c4docobs_free(docObserver);
        c4dbobs_free(dbObserver);
    }

    void dbObserverCalled(C4DatabaseObserver *obs) {
        CHECK(obs == dbObserver);
        ++dbCallbackCalls;
    }

    void docObserverCalled(C4DocumentObserver* obs,
                           C4Slice docID,
                           C4SequenceNumber seq)
    {
        CHECK(obs == docObserver);
        ++docCallbackCalls;
    }

    void checkChanges(std::vector<const char*> expectedDocIDs,
                      std::vector<const char*> expectedRevIDs,
                      bool expectedExternal =false) {
        C4DatabaseChange changes[100];
        bool external;
        auto changeCount = c4dbobs_getChanges(dbObserver, changes, 100, &external);
        REQUIRE(changeCount == expectedDocIDs.size());
        for (unsigned i = 0; i < changeCount; ++i) {
            CHECK(changes[i].docID == c4str(expectedDocIDs[i]));
            CHECK(changes[i].revID == c4str(expectedRevIDs[i]));
            i++;
        }
        CHECK(external == expectedExternal);
    }

    C4DatabaseObserver* dbObserver {nullptr};
    unsigned dbCallbackCalls {0};

    C4DocumentObserver* docObserver {nullptr};
    unsigned docCallbackCalls {0};
};


static void dbObserverCallback(C4DatabaseObserver* obs, void *context) {
    ((C4ObserverTest*)context)->dbObserverCalled(obs);
}

static void docObserverCallback(C4DocumentObserver* obs,
                                C4Slice docID,
                                C4SequenceNumber seq,
                                void *context)
{
    ((C4ObserverTest*)context)->docObserverCalled(obs, docID, seq);
}


TEST_CASE_METHOD(C4ObserverTest, "DB Observer", "[Observer][C]") {
    dbObserver = c4dbobs_create(db, dbObserverCallback, this);
    CHECK(dbCallbackCalls == 0);

    createRev(C4STR("A"), C4STR("1-aa"), kBody);
    CHECK(dbCallbackCalls == 1);
    createRev(C4STR("B"), C4STR("1-bb"), kBody);
    CHECK(dbCallbackCalls == 1);

    checkChanges({"A", "B"}, {"1-aa", "1-bb"});

    createRev(C4STR("B"), C4STR("2-bbbb"), kBody);
    CHECK(dbCallbackCalls == 2);
    createRev(C4STR("C"), C4STR("1-cc"), kBody);
    CHECK(dbCallbackCalls == 2);

    checkChanges({"B", "C"}, {"2-bbbb", "1-cc"});

    c4dbobs_free(dbObserver);
    dbObserver = nullptr;

    createRev(C4STR("A"), C4STR("2-aaaa"), kBody);
    CHECK(dbCallbackCalls == 2);
}


TEST_CASE_METHOD(C4ObserverTest, "Doc Observer", "[Observer][C]") {
    createRev(C4STR("A"), C4STR("1-aa"), kBody);

    docObserver = c4docobs_create(db, C4STR("A"), docObserverCallback, this);
    CHECK(docCallbackCalls == 0);

    createRev(C4STR("A"), C4STR("2-bb"), kBody);
    createRev(C4STR("B"), C4STR("1-bb"), kBody);
    CHECK(docCallbackCalls == 1);
}


TEST_CASE_METHOD(C4ObserverTest, "Multi-DB Observer", "[Observer][C]") {
    dbObserver = c4dbobs_create(db, dbObserverCallback, this);
    CHECK(dbCallbackCalls == 0);

    createRev(C4STR("A"), C4STR("1-aa"), kBody);
    CHECK(dbCallbackCalls == 1);
    createRev(C4STR("B"), C4STR("1-bb"), kBody);
    CHECK(dbCallbackCalls == 1);
    checkChanges({"A", "B"}, {"1-aa", "1-bb"});

    // Open another database on the same file:
    C4Database* otherdb = c4db_open(databasePath(), c4db_getConfig(db), nullptr);
    REQUIRE(otherdb);
    {
        TransactionHelper t(otherdb);
        createRev(otherdb, C4STR("c"), C4STR("1-cc"), kBody);
        createRev(otherdb, C4STR("d"), C4STR("1-dd"), kBody);
        createRev(otherdb, C4STR("e"), C4STR("1-ee"), kBody);
    }

    CHECK(dbCallbackCalls == 2);

    checkChanges({"c", "d", "e"}, {"1-cc", "1-dd", "1-ee"}, true);

    c4dbobs_free(dbObserver);
    dbObserver = nullptr;

    createRev(C4STR("A"), C4STR("2-aaaa"), kBody);
    CHECK(dbCallbackCalls == 2);

    c4db_close(otherdb, NULL);
    c4db_free(otherdb);
}
