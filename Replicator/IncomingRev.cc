//
//  IncomingRev.cc
//  LiteCore
//
//  Created by Jens Alfke on 3/30/17.
//  Copyright © 2017 Couchbase. All rights reserved.
//

#include "IncomingRev.hh"
#include "IncomingBlob.hh"
#include "DBWorker.hh"
#include "Puller.hh"
#include "StringUtil.hh"
#include "c4Document+Fleece.h"
#include "BLIP.hh"
#include <deque>

using namespace std;
using namespace fleece;
using namespace fleeceapi;
using namespace litecore::blip;

namespace litecore { namespace repl {


    IncomingRev::IncomingRev(Puller *puller, DBWorker *dbWorker)
    :Worker(puller, "inc")
    ,_puller(puller)
    ,_dbWorker(dbWorker)
    {
        _important = false;
    }


    // Resets the object so it can be reused for another revision.
    void IncomingRev::clear() {
        assert(_pendingCallbacks == 0 && _pendingBlobs == 0);
        _revMessage = nullptr;
        _rev.clear();
        _error = {};
    }

    
    // Read the 'rev' message, on my actor thread:
    void IncomingRev::_handleRev(Retained<blip::MessageIn> msg) {
        assert(!_revMessage);
        _parent = _puller;  // Necessary because Worker clears _parent when first completed

        _revMessage = msg;
        _rev.docID = _revMessage->property("id"_sl);
        _rev.revID = _revMessage->property("rev"_sl);
        if (_revMessage->property("deleted"_sl))
            _rev.flags |= kRevDeleted;
        _rev.historyBuf = _revMessage->property("history"_sl);
        slice sequence(_revMessage->property("sequence"_sl));

        _peerError = (int)_revMessage->intProperty("error"_sl);
        if (_peerError) {
            // The sender had a last-minute failure getting the promised revision. Give up.
            warn("Peer was unable to send '%.*s'/%.*s: error %d",
                 SPLAT(_rev.docID), SPLAT(_rev.revID), _peerError);
            finish();
            return;
        }

        // Validate the revID and sequence:
        logVerbose("Received revision '%.*s' #%.*s (seq '%.*s')",
                   SPLAT(_rev.docID), SPLAT(_rev.revID), SPLAT(sequence));
        if (_rev.docID.size == 0 || _rev.revID.size == 0) {
            warn("Got invalid revision");
            _error = c4error_make(WebSocketDomain, 400, "received invalid revision"_sl);
            finish();
            return;
        }
        if (!sequence && nonPassive()) {
            warn("Missing sequence in 'rev' message for active puller");
            _error = c4error_make(WebSocketDomain, 400,
                                  "received revision with missing 'sequence'"_sl);
            finish();
            return;
        }

        // Parse the JSON to Fleece. This Fleece data is _not_ suitable for inserting into the
        // database because it doesn't use the SharedKeys, but it lets us look at the doc
        // metadata and blobs.
        FLError err;
        alloc_slice fleeceBody = Encoder::convertJSON(_revMessage->body(), &err);
        if (!fleeceBody) {
            _error = {FleeceDomain, err};
            finish();
            return;
        }
        Dict root = Value::fromTrustedData(fleeceBody).asDict();

        // Strip out any "_"-prefixed properties like _id, just in case, and also any attachments
        // in _attachments that are redundant with blobs elsewhere in the doc:
        if (c4doc_hasOldMetaProperties(root)) {
            fleeceBody = c4doc_encodeStrippingOldMetaProperties(root);
            root = Value::fromTrustedData(fleeceBody).asDict();
        }

        // Populate the RevToInsert's body:
        _rev.body = fleeceBody;

        // Call the custom validation function if any:
        if (_options.pullValidator) {
            if (!_options.pullValidator(_rev.docID, root, _options.pullValidatorContext)) {
                log("Rejected by pull validator function");
                _error = c4error_make(WebSocketDomain, 403, "rejected by validation function"_sl);
                finish();
                return;
            }
        }

        // Check for blobs, and request any I don't have yet:
        auto blobStore = _dbWorker->blobStore();
        findBlobReferences(root, nullptr, [=](Dict dict, const C4BlobKey &key) {
            if (c4blob_getSize(blobStore, key) < 0) {
                uint64_t length = dict["length"_sl].asUnsigned();
                Retained<IncomingBlob> b(new IncomingBlob(this, blobStore));
                b->start(key, length);
                ++_pendingBlobs;
            }
        });
        if (_pendingBlobs > 0)
            _rev.flags |= kRevHasAttachments;
        else
            insertRevision();
    }


    void IncomingRev::_childChangedStatus(Worker *task, Status status) {
        addProgress(status.progressDelta);
        if (status.level == kC4Stopped) {
            if (status.error.code && !_error.code)
                _error = status.error;
            assert(_pendingBlobs > 0);
            if (--_pendingBlobs == 0) {
                // All blobs completed, now finish:
                if (_error.code == 0) {
                    logVerbose("All blobs received, now inserting revision");
                    insertRevision();
                } else {
                    finish();
                }
            }
        }
    }


    // Asks the DBAgent to insert the revision, then sends the reply and notifies the Puller.
    void IncomingRev::insertRevision() {
        ++_pendingCallbacks;
        _rev.onInserted = asynchronize([this](C4Error err) {
            // Callback that will run _after_ insertRevision() completes:
            --_pendingCallbacks;
            _error = err;
            finish();
        });

        _dbWorker->insertRevision(&_rev);
    }


    void IncomingRev::finish() {
        if (!_revMessage->noReply()) {
            MessageBuilder response(_revMessage);
            if (_error.code != 0)
                response.makeError(c4ToBLIPError(_error));
            _revMessage->respond(response);
        }
        if (_error.code == 0 && _peerError)
            _error = c4error_make(WebSocketDomain, 502, "Peer failed to send revision"_sl);
        if (_error.code)
            gotDocumentError(_rev.docID, _error, false, false);
        _puller->revWasHandled(this, _rev.docID, remoteSequence(), (_error.code == 0));
        clear();
    }


    Worker::ActivityLevel IncomingRev::computeActivityLevel() const {
        if (Worker::computeActivityLevel() == kC4Busy
                || _pendingCallbacks > 0 || _pendingBlobs > 0) {
            return kC4Busy;
        } else {
            return kC4Stopped;
        }
    }


#pragma mark - UTILITIES:


    static inline void pushIfDictOrArray(Value v, deque<Value> &stack) {
        auto type = v.type();
        if (type == kFLDict || type == kFLArray)
            stack.push_front(v);
    }


    // Finds blob references anywhere in a Fleece value
    void IncomingRev::findBlobReferences(Dict root, FLSharedKeys sk, const FindBlobCallback &callback) {
        Value val = root;
        deque<Value> stack;
        while(true) {
            auto dict = val.asDict();
            if (dict) {
                C4BlobKey blobKey;
                if (c4doc_dictIsBlob(dict, sk, &blobKey)) {
                    callback(dict, blobKey);
                } else {
                    for (Dict::iterator i(dict); i; ++i)
                        pushIfDictOrArray(i.value(), stack);
                }
            } else {
                for (Array::iterator i(val.asArray()); i; ++i)
                    pushIfDictOrArray(i.value(), stack);
            }

            // Get next value from queue, or stop when we run out:
            if (stack.empty())
                break;
            val = stack.front();
            stack.pop_front();
        }
    }

} }

